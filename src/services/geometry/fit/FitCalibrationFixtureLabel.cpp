#include "FitCalibrationFixtureLabel.h"
#include "FitCalibrationNamingCatalog.h"

#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"

#include <QHash>
#include <QVector>
#include <array>
#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double PixelMillimetres = .4;
constexpr double BottomZ = -.8;
constexpr double OutsideZ = -.2;
constexpr double LetterZ = .45;

std::array<quint8, 7> glyph(QChar character)
{
    switch (character.toLatin1()) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {31,4,4,4,4,4,31};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '-': return {0,0,0,31,0,0,0};
    case ' ': return {0,0,0,0,0,0,0};
    default: return {31,1,2,4,4,0,4};
    }
}

bool fail(QString* error, const QString& message)
{
    if (error) *error = message;
    return false;
}

PrintMesh cutterFor(const QVector<quint8>& pixels, int width, int height,
                    const FitFixtureLabelRegion& region)
{
    PrintMesh mesh;
    QHash<quint64, std::uint32_t> vertices;
    const auto vertex = [&](int x, int y, int level) -> std::uint32_t {
        const quint64 key = (quint64(x) * quint64(height+1) + quint64(y)) * 3 + quint64(level);
        const auto found = vertices.constFind(key);
        if (found != vertices.cend()) return found.value();
        const auto index = std::uint32_t(mesh.vertices.size());
        mesh.vertices.push_back({region.minimumX+x*PixelMillimetres,
                                 region.minimumY+y*PixelMillimetres,
                                 level == 0 ? BottomZ : level == 1 ? OutsideZ : LetterZ});
        vertices.insert(key, index);
        return index;
    };
    const auto quad = [&](std::uint32_t a, std::uint32_t b,
                          std::uint32_t c, std::uint32_t d) {
        mesh.faces.push_back({a,b,c});
        mesh.faces.push_back({a,c,d});
    };
    const auto active = [&](int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height && pixels[y*width+x] != 0;
    };
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const int top = active(x,y) ? 2 : 1;
        quad(vertex(x,y,top),vertex(x+1,y,top),vertex(x+1,y+1,top),vertex(x,y+1,top));
        quad(vertex(x,y,0),vertex(x,y+1,0),vertex(x+1,y+1,0),vertex(x+1,y,0));
        if (x == 0) {
            quad(vertex(x,y,0),vertex(x,y,1),vertex(x,y+1,1),vertex(x,y+1,0));
            if (top == 2) quad(vertex(x,y,1),vertex(x,y,2),vertex(x,y+1,2),vertex(x,y+1,1));
        } else if (top > (active(x-1,y)?2:1))
            quad(vertex(x,y,1),vertex(x,y,2),vertex(x,y+1,2),vertex(x,y+1,1));
        if (x == width-1) {
            quad(vertex(x+1,y,0),vertex(x+1,y+1,0),vertex(x+1,y+1,1),vertex(x+1,y,1));
            if (top == 2) quad(vertex(x+1,y,1),vertex(x+1,y+1,1),vertex(x+1,y+1,2),vertex(x+1,y,2));
        } else if (top > (active(x+1,y)?2:1))
            quad(vertex(x+1,y,1),vertex(x+1,y+1,1),vertex(x+1,y+1,2),vertex(x+1,y,2));
        if (y == 0) {
            quad(vertex(x,y,0),vertex(x+1,y,0),vertex(x+1,y,1),vertex(x,y,1));
            if (top == 2) quad(vertex(x,y,1),vertex(x+1,y,1),vertex(x+1,y,2),vertex(x,y,2));
        } else if (top > (active(x,y-1)?2:1))
            quad(vertex(x,y,1),vertex(x+1,y,1),vertex(x+1,y,2),vertex(x,y,2));
        if (y == height-1) {
            quad(vertex(x,y+1,0),vertex(x,y+1,1),vertex(x+1,y+1,1),vertex(x+1,y+1,0));
            if (top == 2) quad(vertex(x,y+1,1),vertex(x,y+1,2),vertex(x+1,y+1,2),vertex(x+1,y+1,1));
        } else if (top > (active(x,y+1)?2:1))
            quad(vertex(x,y+1,1),vertex(x,y+1,2),vertex(x+1,y+1,2),vertex(x+1,y+1,1));
    }
    if (analyzeSource(mesh).signedVolume < 0)
        for (auto& face : mesh.faces) std::swap(face[1], face[2]);
    return mesh;
}
} // namespace

bool FitCalibrationFixtureLabel::recess(const PrintMesh& source, const QString& displayLabel,
    const QString& shortLabel, const FitFixtureLabelRegion& region,
    PrintMesh* labeled, QString* appliedLabel, QString* error)
{
    const auto sourceAnalysis = analyzeSource(source);
    if (!labeled || !std::isfinite(region.minimumX) || !std::isfinite(region.maximumX) ||
        !std::isfinite(region.minimumY) || !std::isfinite(region.maximumY) ||
        region.maximumX-region.minimumX < 20 || region.maximumY-region.minimumY < 3.2 ||
        !validatePreparedMesh(sourceAnalysis).ok() ||
        region.minimumX < sourceAnalysis.bounds.minimum.x ||
        region.maximumX > sourceAnalysis.bounds.maximum.x ||
        region.minimumY < sourceAnalysis.bounds.minimum.y ||
        region.maximumY > sourceAnalysis.bounds.maximum.y)
        return fail(error, QStringLiteral("A valid flat-base fixture and safe label region are required."));
    const int availableWidth = int(std::floor((region.maximumX-region.minimumX)/PixelMillimetres));
    const int availableHeight = int(std::floor((region.maximumY-region.minimumY)/PixelMillimetres));
    QString text = displayLabel.toUpper().replace(QChar(0x2014), QChar('-'));
    if (text.size()*6+4 > availableWidth)
        text = shortLabel.toUpper().replace(QChar(0x2014), QChar('-'));
    if (text.trimmed().isEmpty() || text.size()*6+4 > availableWidth || availableHeight < 9)
        return fail(error, QStringLiteral("The feature label does not fit the protected base area."));
    const int width = text.size()*6+4, height = 9;
    QVector<quint8> pixels(width*height);
    int marked = 0;
    for (int character = 0; character < text.size(); ++character) {
        const auto rows = glyph(text[character]);
        for (int y = 0; y < 7; ++y) for (int x = 0; x < 5; ++x)
            if (rows[y] & (1 << (4-x))) { pixels[(y+1)*width+character*6+x+2] = 1; ++marked; }
    }
    if (marked < 12) return fail(error, QStringLiteral("The fixture label is not readable at the supported resolution."));
    // Diagonal-only voxel contacts would make a non-manifold cutter. Bridge them
    // within the lettering mask without entering any feature geometry.
    for (int y = 0; y+1 < height; ++y) for (int x = 0; x+1 < width; ++x) {
        const int a = y*width+x, b = a+1, c = a+width, d = c+1;
        if (pixels[a] && pixels[d] && !pixels[b] && !pixels[c]) pixels[b] = 1;
        if (pixels[b] && pixels[c] && !pixels[a] && !pixels[d]) pixels[a] = 1;
    }
    const auto cutter = cutterFor(pixels, width, height, region);
    if (!validatePreparedMesh(analyzeSource(cutter)).ok())
        return fail(error, QStringLiteral("The recessed-letter cutter is not manifold."));
    const auto cut = McutMeshBooleanService().subtract(source, cutter);
    const auto& bounds = cut.resultAnalysis.bounds;
    const auto& originalBounds = sourceAnalysis.bounds;
    const auto same = [](double a, double b) { return std::abs(a-b) < .001; };
    if (!cut.ok() || !validatePreparedMesh(cut.resultAnalysis).ok() ||
        !same(bounds.minimum.x, originalBounds.minimum.x) ||
        !same(bounds.minimum.y, originalBounds.minimum.y) ||
        !same(bounds.minimum.z, originalBounds.minimum.z) ||
        !same(bounds.maximum.x, originalBounds.maximum.x) ||
        !same(bounds.maximum.y, originalBounds.maximum.y) ||
        !same(bounds.maximum.z, originalBounds.maximum.z) ||
        cut.resultAnalysis.absoluteVolume >= sourceAnalysis.absoluteVolume)
        return fail(error, QStringLiteral("The label could not be recessed without changing fixture validity."));
    *labeled = cut.mesh;
    if (appliedLabel) *appliedLabel = text;
    if (error) error->clear();
    return true;
}

bool FitCalibrationFixtureLabel::recessStandalone(const PrintMesh& source, FitCalibrationNameKey key,
    PrintMesh* labeled, QString* appliedLabel, QString* error)
{
    const auto bounds = analyzeSource(source).bounds;
    FitFixtureLabelRegion region{6, bounds.maximum.x - 6, .5, 4.5};
    if (key == FitCalibrationNameKey::ClutchTubeWall) region = {6, bounds.maximum.x - 6, 3, 8};
    else if (key == FitCalibrationNameKey::ClutchPostWall) region = {6, bounds.maximum.x - 6, 1, 5};
    const auto name = FitCalibrationNamingCatalog::forKey(key);
    return recess(source, QString::fromUtf8(name.canonical), QString::fromLatin1(name.abbreviated),
                  region, labeled, appliedLabel, error);
}

} // namespace PrintGeometry
