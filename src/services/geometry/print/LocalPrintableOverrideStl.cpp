#include "LocalPrintableOverrideService.h"
#include <QDataStream>
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>

namespace PrintGeometry {
namespace {
constexpr qint64 MaximumBytes=64*1024*1024;
constexpr std::size_t MaximumFaces=100000;
constexpr std::size_t MaximumVertices=300000;
bool space(char c){return c==' '||(c>='\t'&&c<='\r');}

class AsciiTokens
{
public:
    explicit AsciiTokens(const QByteArray& data):m_data(data){}
    QByteArray next()
    {
        while(m_at<m_data.size()&&space(m_data[m_at]))++m_at;
        const auto begin=m_at;
        while(m_at<m_data.size()&&!space(m_data[m_at])){
            if(m_at-begin>=128)throw std::runtime_error("ASCII STL token exceeds 128 bytes.");
            ++m_at;
        }
        return m_data.mid(begin,m_at-begin);
    }
    void expect(const char* keyword)
    {
        if(next()!=keyword)throw std::runtime_error("Malformed ASCII STL: expected "+std::string(keyword)+".");
    }
    double number()
    {
        bool ok=false;const auto token=next();const double value=token.toDouble(&ok);
        if(!ok||!std::isfinite(value))throw std::runtime_error("ASCII STL contains an invalid or non-finite numeric field.");
        return value;
    }
    void skipName()
    {
        // solid/endsolid names extend to the end of their line; names are not geometry.
        while(m_at<m_data.size()&&m_data[m_at]!='\n'&&m_data[m_at]!='\r')++m_at;
    }
private:
    const QByteArray& m_data;
    qsizetype m_at=0;
};

class Facets
{
public:
    PrintMesh mesh;
    void append(const std::array<Point,3>& points)
    {
        if(mesh.faces.size()>=MaximumFaces)throw std::runtime_error("STL exceeds the 100,000 triangle import limit.");
        Face face;
        for(int i=0;i<3;++i){
            const auto& p=points[i];
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>100000)
                throw std::runtime_error("STL contains invalid or excessive coordinates; expected millimeters.");
            // STL repeats vertex coordinates per facet. Share only numerically
            // identical positions, never a proximity weld. Preserve EVERY face,
            // including duplicate/degenerate faces, for the common validator.
            const std::array<double,3> key{p.x,p.y,p.z};
            const auto inserted=m_vertices.emplace(key,std::uint32_t(mesh.vertices.size()));
            if(inserted.second){
                if(mesh.vertices.size()>=MaximumVertices)throw std::runtime_error("STL exceeds the 300,000 vertex import limit.");
                mesh.vertices.push_back(p);
            }
            face[i]=inserted.first->second;
        }
        mesh.faces.push_back(face);
    }
private:
    std::map<std::array<double,3>,std::uint32_t> m_vertices;
};
}

bool LocalPrintableOverrideService::readStl(const QString& path,PrintMesh* mesh,QString* error)
{
    try {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("STL file could not be opened.");
        const auto data=file.read(MaximumBytes+1);
        if(data.isEmpty()||data.size()>MaximumBytes||file.error()!=QFileDevice::NoError)throw std::runtime_error("STL is empty, unreadable, or exceeds the 64 MiB import limit.");
        const quint32 count=data.size()>=84?qFromLittleEndian<quint32>(data.constData()+80):0;
        const bool binary=data.size()>=84&&quint64(data.size())==84+quint64(count)*50;
        Facets result;
        if(binary){
            // Exact byte length takes precedence over a misleading 'solid' header.
            if(count==0||count>MaximumFaces)throw std::runtime_error("Binary STL triangle count is empty or exceeds the 100,000 triangle limit.");
            QDataStream stream(data);stream.setByteOrder(QDataStream::LittleEndian);stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
            stream.skipRawData(84);
            for(quint32 i=0;i<count;++i){
                float x,y,z;stream>>x>>y>>z;
                if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))throw std::runtime_error("Binary STL contains a non-finite facet normal.");
                std::array<Point,3> points;
                for(auto& p:points){stream>>x>>y>>z;p={x,y,z};}
                quint16 attributes=0;stream>>attributes; // Opaque color/attribute word, not geometry.
                if(stream.status()!=QDataStream::Ok)throw std::runtime_error("Binary STL facet is truncated; no partial mesh was loaded.");
                result.append(points);
            }
            if(!stream.atEnd())throw std::runtime_error("Unexpected trailing binary STL data.");
        }else{
            AsciiTokens tokens(data);
            if(tokens.next()!="solid")throw std::runtime_error("STL is not a complete ASCII solid or its binary triangle count/byte length is inconsistent; no partial mesh was loaded.");
            tokens.skipName();
            for(;;){
                const auto keyword=tokens.next();
                if(keyword=="endsolid"){
                    tokens.skipName();
                    if(!tokens.next().isEmpty())throw std::runtime_error("Trailing data or multiple ASCII solid blocks are unsupported; export one STL solid block.");
                    break;
                }
                if(keyword!="facet")throw std::runtime_error("Malformed or incomplete ASCII STL: expected facet or endsolid; no partial mesh was loaded.");
                tokens.expect("normal");for(int i=0;i<3;++i)tokens.number();
                tokens.expect("outer");tokens.expect("loop");
                std::array<Point,3> points;
                for(auto& p:points){tokens.expect("vertex");p.x=tokens.number();p.y=tokens.number();p.z=tokens.number();}
                tokens.expect("endloop");tokens.expect("endfacet");result.append(points);
            }
        }
        if(result.mesh.faces.empty())throw std::runtime_error("STL contains no triangles.");
        *mesh=std::move(result.mesh);return true;
    }catch(const std::exception& e){if(error)*error=QString::fromUtf8(e.what());return false;}
}
} // namespace PrintGeometry
