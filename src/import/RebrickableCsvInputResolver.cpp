/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "RebrickableCsvInputResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtEndian>

#include <zlib.h>

#include <limits>

namespace {

constexpr quint32 LocalFileHeaderSignature = 0x04034b50;
constexpr quint32 CentralDirectoryHeaderSignature = 0x02014b50;
constexpr quint32 EndOfCentralDirectorySignature = 0x06054b50;
constexpr int EndOfCentralDirectoryMinimumSize = 22;
constexpr int MaximumZipCommentSize = 65535;
constexpr qsizetype IoBufferSize = 64 * 1024;

// Rebrickable Parts/Sets CSV files are normally far below these limits. The
// margins accommodate substantial future catalog growth while preventing a
// selected archive from requesting effectively unbounded memory or disk use.
constexpr quint32 MaximumCentralDirectorySize = 32 * 1024 * 1024;
constexpr quint16 MaximumZipEntryCount = 10000;
constexpr quint64 MaximumExtractedCsvSize = 1024ULL * 1024 * 1024;

struct ZipEntry
{
    QString fileName;
    quint16 flags = 0;
    quint16 compressionMethod = 0;
    quint32 crc = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

quint16 readUInt16(const QByteArray& data, qsizetype offset)
{
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint32 readUInt32(const QByteArray& data, qsizetype offset)
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(data.constData() + offset));
}

bool readExactly(QFile& file, qint64 size, QByteArray& data)
{
    data = file.read(size);

    return data.size() == size;
}

bool findEndOfCentralDirectory(QFile& file,
                               quint16& entryCount,
                               quint32& directorySize,
                               quint32& directoryOffset,
                               QString& errorMessage)
{
    const qint64 searchSize = qMin(file.size(),
                                   qint64(EndOfCentralDirectoryMinimumSize
                                          + MaximumZipCommentSize));

    if (!file.seek(file.size() - searchSize)) {
        errorMessage = "Unable to read the ZIP directory.";
        return false;
    }

    QByteArray tail;

    if (!readExactly(file, searchSize, tail)) {
        errorMessage = "The ZIP file is truncated or unreadable.";
        return false;
    }

    for (qsizetype offset = tail.size() - EndOfCentralDirectoryMinimumSize;
         offset >= 0;
         --offset) {
        if (readUInt32(tail, offset) != EndOfCentralDirectorySignature)
            continue;

        const quint16 commentLength = readUInt16(tail, offset + 20);

        if (offset + EndOfCentralDirectoryMinimumSize + commentLength != tail.size())
            continue;

        const quint16 diskNumber = readUInt16(tail, offset + 4);
        const quint16 directoryDisk = readUInt16(tail, offset + 6);
        const quint16 entriesOnDisk = readUInt16(tail, offset + 8);

        entryCount = readUInt16(tail, offset + 10);
        directorySize = readUInt32(tail, offset + 12);
        directoryOffset = readUInt32(tail, offset + 16);

        if (diskNumber != 0 || directoryDisk != 0 || entriesOnDisk != entryCount) {
            errorMessage = "Multi-volume ZIP files are not supported.";
            return false;
        }

        if (entryCount == std::numeric_limits<quint16>::max()
            || directorySize == std::numeric_limits<quint32>::max()
            || directoryOffset == std::numeric_limits<quint32>::max()) {
            errorMessage = "ZIP64 archives are not supported for Rebrickable imports.";
            return false;
        }

        if (entryCount > MaximumZipEntryCount) {
            errorMessage = QString("The ZIP file contains too many entries (maximum %1).")
                               .arg(MaximumZipEntryCount);
            return false;
        }

        if (directorySize > MaximumCentralDirectorySize) {
            errorMessage = QString("The ZIP directory is too large (maximum %1 MiB).")
                               .arg(MaximumCentralDirectorySize / (1024 * 1024));
            return false;
        }

        if (qint64(directoryOffset) + directorySize > file.size()) {
            errorMessage = "The ZIP directory is corrupt.";
            return false;
        }

        return true;
    }

    errorMessage = "The selected file is not a valid ZIP archive.";
    return false;
}

bool readEntries(QFile& file, QList<ZipEntry>& entries, QString& errorMessage)
{
    quint16 entryCount = 0;
    quint32 directorySize = 0;
    quint32 directoryOffset = 0;

    if (!findEndOfCentralDirectory(file,
                                   entryCount,
                                   directorySize,
                                   directoryOffset,
                                   errorMessage)) {
        return false;
    }

    if (!file.seek(directoryOffset)) {
        errorMessage = "Unable to read the ZIP directory.";
        return false;
    }

    QByteArray directory;

    if (!readExactly(file, directorySize, directory)) {
        errorMessage = "The ZIP directory is truncated or unreadable.";
        return false;
    }

    qsizetype offset = 0;

    for (quint16 index = 0; index < entryCount; ++index) {
        constexpr qsizetype fixedHeaderSize = 46;

        if (offset + fixedHeaderSize > directory.size()
            || readUInt32(directory, offset) != CentralDirectoryHeaderSignature) {
            errorMessage = "The ZIP directory contains an invalid entry.";
            return false;
        }

        const quint16 fileNameLength = readUInt16(directory, offset + 28);
        const quint16 extraLength = readUInt16(directory, offset + 30);
        const quint16 commentLength = readUInt16(directory, offset + 32);
        const qsizetype entrySize = fixedHeaderSize + fileNameLength + extraLength
                                    + commentLength;

        if (offset + entrySize > directory.size()) {
            errorMessage = "The ZIP directory contains a truncated entry.";
            return false;
        }

        ZipEntry entry;
        entry.flags = readUInt16(directory, offset + 8);
        entry.compressionMethod = readUInt16(directory, offset + 10);
        entry.crc = readUInt32(directory, offset + 16);
        entry.compressedSize = readUInt32(directory, offset + 20);
        entry.uncompressedSize = readUInt32(directory, offset + 24);
        entry.localHeaderOffset = readUInt32(directory, offset + 42);

        if (entry.compressedSize == std::numeric_limits<quint32>::max()
            || entry.uncompressedSize == std::numeric_limits<quint32>::max()
            || entry.localHeaderOffset == std::numeric_limits<quint32>::max()) {
            errorMessage = "ZIP64 entries are not supported for Rebrickable imports.";
            return false;
        }

        const QByteArray encodedName = directory.mid(offset + fixedHeaderSize,
                                                     fileNameLength);
        entry.fileName = QString::fromUtf8(encodedName);

        if (!entry.fileName.endsWith('/') && !entry.fileName.endsWith('\\'))
            entries.append(entry);

        offset += entrySize;
    }

    return true;
}

bool canWriteOutput(quint64 bytesWritten,
                    quint64 nextChunkSize,
                    const ZipEntry& entry,
                    QString& errorMessage)
{
    if (bytesWritten > entry.uncompressedSize
        || nextChunkSize > quint64(entry.uncompressedSize) - bytesWritten) {
        errorMessage = "The CSV entry expands beyond its declared uncompressed size.";
        return false;
    }

    if (bytesWritten > MaximumExtractedCsvSize
        || nextChunkSize > MaximumExtractedCsvSize - bytesWritten) {
        errorMessage = QString("The extracted CSV exceeds the %1 MiB safety limit.")
                           .arg(MaximumExtractedCsvSize / (1024 * 1024));
        return false;
    }

    return true;
}

bool writeStoredData(QFile& archive,
                     QSaveFile& output,
                     const ZipEntry& entry,
                     uLong& calculatedCrc,
                     quint64& bytesWritten,
                     QString& errorMessage)
{
    if (entry.compressedSize != entry.uncompressedSize) {
        errorMessage = "The stored ZIP entry has inconsistent sizes.";
        return false;
    }

    quint64 remaining = entry.compressedSize;

    while (remaining > 0) {
        const qint64 chunkSize = qMin<quint64>(remaining, IoBufferSize);
        const QByteArray data = archive.read(chunkSize);

        if (data.size() != chunkSize) {
            errorMessage = "The CSV entry in the ZIP file is truncated.";
            return false;
        }

        if (!canWriteOutput(bytesWritten,
                            static_cast<quint64>(data.size()),
                            entry,
                            errorMessage)) {
            return false;
        }

        if (output.write(data) != data.size()) {
            errorMessage = "Unable to write the temporary CSV file.";
            return false;
        }

        calculatedCrc = crc32(calculatedCrc,
                              reinterpret_cast<const Bytef*>(data.constData()),
                              static_cast<uInt>(data.size()));
        bytesWritten += data.size();
        remaining -= data.size();
    }

    return true;
}

bool writeDeflatedData(QFile& archive,
                       QSaveFile& output,
                       const ZipEntry& entry,
                       uLong& calculatedCrc,
                       quint64& bytesWritten,
                       QString& errorMessage)
{
    z_stream stream = {};

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        errorMessage = "Unable to initialize ZIP decompression.";
        return false;
    }

    QByteArray input(IoBufferSize, Qt::Uninitialized);
    QByteArray outputBuffer(IoBufferSize, Qt::Uninitialized);
    quint64 remaining = entry.compressedSize;
    int inflateResult = Z_OK;

    while (inflateResult != Z_STREAM_END) {
        if (stream.avail_in == 0 && remaining > 0) {
            const qint64 chunkSize = qMin<quint64>(remaining, input.size());
            const qint64 bytesRead = archive.read(input.data(), chunkSize);

            if (bytesRead != chunkSize) {
                inflateEnd(&stream);
                errorMessage = "The compressed CSV entry is truncated.";
                return false;
            }

            stream.next_in = reinterpret_cast<Bytef*>(input.data());
            stream.avail_in = static_cast<uInt>(bytesRead);
            remaining -= bytesRead;
        }

        stream.next_out = reinterpret_cast<Bytef*>(outputBuffer.data());
        stream.avail_out = static_cast<uInt>(outputBuffer.size());
        inflateResult = inflate(&stream, Z_NO_FLUSH);

        if (inflateResult != Z_OK && inflateResult != Z_STREAM_END) {
            inflateEnd(&stream);
            errorMessage = "The CSV entry in the ZIP file is corrupt.";
            return false;
        }

        const qsizetype produced = outputBuffer.size() - stream.avail_out;

        if (produced > 0) {
            if (!canWriteOutput(bytesWritten,
                                static_cast<quint64>(produced),
                                entry,
                                errorMessage)) {
                inflateEnd(&stream);
                return false;
            }

            if (output.write(outputBuffer.constData(), produced) != produced) {
                inflateEnd(&stream);
                errorMessage = "Unable to write the temporary CSV file.";
                return false;
            }

            calculatedCrc = crc32(
                calculatedCrc,
                reinterpret_cast<const Bytef*>(outputBuffer.constData()),
                static_cast<uInt>(produced));
            bytesWritten += produced;
        }

        if (stream.avail_in == 0 && remaining == 0 && inflateResult != Z_STREAM_END
            && produced == 0) {
            inflateEnd(&stream);
            errorMessage = "The compressed CSV entry is incomplete.";
            return false;
        }
    }

    const bool consumedAllInput = remaining == 0 && stream.avail_in == 0;
    inflateEnd(&stream);

    if (!consumedAllInput) {
        errorMessage = "The compressed CSV entry contains invalid trailing data.";
        return false;
    }

    return true;
}

bool extractEntry(QFile& archive,
                  const ZipEntry& entry,
                  const QString& outputFileName,
                  QString& errorMessage)
{
    constexpr quint16 TraditionalEncryptionFlag = 0x0001;
    constexpr quint16 StrongEncryptionFlag = 0x0040;

    if (entry.flags & (TraditionalEncryptionFlag | StrongEncryptionFlag)) {
        errorMessage = "Encrypted ZIP entries are not supported.";
        return false;
    }

    if (entry.compressionMethod != 0 && entry.compressionMethod != 8) {
        errorMessage = QString("The CSV entry uses unsupported ZIP compression method %1.")
                           .arg(entry.compressionMethod);
        return false;
    }

    if (!archive.seek(entry.localHeaderOffset)) {
        errorMessage = "Unable to locate the CSV entry in the ZIP file.";
        return false;
    }

    QByteArray localHeader;

    if (!readExactly(archive, 30, localHeader)
        || readUInt32(localHeader, 0) != LocalFileHeaderSignature) {
        errorMessage = "The ZIP file contains an invalid local entry header.";
        return false;
    }

    const quint16 localFlags = readUInt16(localHeader, 6);
    const quint16 localMethod = readUInt16(localHeader, 8);
    const quint16 fileNameLength = readUInt16(localHeader, 26);
    const quint16 extraLength = readUInt16(localHeader, 28);

    if (localFlags != entry.flags || localMethod != entry.compressionMethod) {
        errorMessage = "The ZIP entry headers are inconsistent.";
        return false;
    }

    const qint64 dataOffset = qint64(entry.localHeaderOffset) + 30
                              + fileNameLength + extraLength;

    if (dataOffset < 0 || dataOffset + entry.compressedSize > archive.size()
        || !archive.seek(dataOffset)) {
        errorMessage = "The CSV entry in the ZIP file is truncated.";
        return false;
    }

    QSaveFile output(outputFileName);

    if (!output.open(QIODevice::WriteOnly)) {
        errorMessage = "Unable to create the temporary CSV file.";
        return false;
    }

    uLong calculatedCrc = crc32(0L, Z_NULL, 0);
    quint64 bytesWritten = 0;
    const bool extracted = entry.compressionMethod == 0
                               ? writeStoredData(archive,
                                                 output,
                                                 entry,
                                                 calculatedCrc,
                                                 bytesWritten,
                                                 errorMessage)
                               : writeDeflatedData(archive,
                                                   output,
                                                   entry,
                                                   calculatedCrc,
                                                   bytesWritten,
                                                   errorMessage);

    if (!extracted)
        return false;

    if (bytesWritten != entry.uncompressedSize
        || static_cast<quint32>(calculatedCrc) != entry.crc) {
        errorMessage = "The extracted CSV failed ZIP integrity validation.";
        return false;
    }

    if (!output.commit()) {
        errorMessage = "Unable to finalize the temporary CSV file.";
        return false;
    }

    return true;
}

} // namespace

bool RebrickableCsvInputResolver::resolve(const QString& inputFileName,
                                          const QString& expectedCsvFileName,
                                          QTemporaryDir& temporaryDirectory,
                                          QString& resolvedCsvFileName,
                                          QString& errorMessage)
{
    resolvedCsvFileName.clear();
    errorMessage.clear();

    const QFileInfo inputInfo(inputFileName);

    if (inputInfo.suffix().compare("csv", Qt::CaseInsensitive) == 0) {
        resolvedCsvFileName = inputFileName;
        return true;
    }

    if (inputInfo.suffix().compare("zip", Qt::CaseInsensitive) != 0) {
        errorMessage = "Select a Rebrickable CSV or ZIP file.";
        return false;
    }

    QFile archive(inputFileName);

    if (!archive.open(QIODevice::ReadOnly)) {
        errorMessage = QString("Unable to open the ZIP file: %1")
                           .arg(archive.errorString());
        return false;
    }

    QList<ZipEntry> entries;

    if (!readEntries(archive, entries, errorMessage))
        return false;

    QList<ZipEntry> csvEntries;
    QList<ZipEntry> exactEntries;

    for (const ZipEntry& entry : entries) {
        QString normalizedEntryName = entry.fileName;
        normalizedEntryName.replace('\\', '/');

        const QFileInfo entryInfo(normalizedEntryName.section('/', -1));

        if (entryInfo.suffix().compare("csv", Qt::CaseInsensitive) != 0)
            continue;

        csvEntries.append(entry);

        if (entryInfo.fileName().compare(expectedCsvFileName, Qt::CaseInsensitive) == 0)
            exactEntries.append(entry);
    }

    if (csvEntries.isEmpty()) {
        errorMessage = QString("The ZIP file does not contain a CSV file usable for %1.")
                           .arg(expectedCsvFileName);
        return false;
    }

    ZipEntry selectedEntry;

    if (exactEntries.size() == 1) {
        selectedEntry = exactEntries.constFirst();
    } else if (exactEntries.size() > 1) {
        errorMessage = QString("The ZIP file contains multiple %1 entries. "
                               "Select an archive with one unambiguous catalog file.")
                           .arg(expectedCsvFileName);
        return false;
    } else if (csvEntries.size() == 1) {
        selectedEntry = csvEntries.constFirst();
    } else {
        errorMessage = QString("The ZIP file contains multiple CSV files but none is named %1. "
                               "BrickSuite cannot safely choose one.")
                           .arg(expectedCsvFileName);
        return false;
    }

    // Never accept traversal/absolute paths, even though extraction uses our
    // own fixed filename rather than an archive-controlled filesystem path.
    QString selectedPath = selectedEntry.fileName;
    selectedPath.replace('\\', '/');
    if (selectedPath.startsWith('/') || selectedPath.contains(':')
        || selectedPath.split('/').contains(QStringLiteral(".."))) {
        errorMessage = "The selected CSV has an unsafe path inside the ZIP file.";
        return false;
    }

    if (selectedEntry.uncompressedSize > MaximumExtractedCsvSize) {
        errorMessage = QString("The selected CSV declares an uncompressed size larger than "
                               "the %1 MiB safety limit.")
                           .arg(MaximumExtractedCsvSize / (1024 * 1024));
        return false;
    }

    if (!temporaryDirectory.isValid()) {
        errorMessage = "Unable to create a temporary directory for ZIP extraction.";
        return false;
    }

    resolvedCsvFileName = temporaryDirectory.filePath(expectedCsvFileName);

    if (!extractEntry(archive, selectedEntry, resolvedCsvFileName, errorMessage)) {
        resolvedCsvFileName.clear();
        return false;
    }

    return true;
}
