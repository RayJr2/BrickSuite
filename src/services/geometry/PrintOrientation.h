#pragma once

#include "print/PrintMesh.h"

#include <QMatrix4x4>
#include <QString>
#include <array>

class PrintOrientation
{
public:
    enum class Rotation { XPositive, XNegative, YPositive, YNegative, ZPositive, ZNegative };

    void rotate(Rotation rotation) { m_matrix = multiply(rotationMatrix(rotation), m_matrix); }
    void reset() { m_matrix = Identity; }
    bool isIdentity() const { return m_matrix == Identity; }

    PrintGeometry::Point map(const PrintGeometry::Point& point) const
    {
        return {double(m_matrix[0])*point.x+double(m_matrix[1])*point.y+double(m_matrix[2])*point.z,
                double(m_matrix[3])*point.x+double(m_matrix[4])*point.y+double(m_matrix[5])*point.z,
                double(m_matrix[6])*point.x+double(m_matrix[7])*point.y+double(m_matrix[8])*point.z};
    }

    PrintGeometry::PrintMesh apply(const PrintGeometry::PrintMesh& source) const
    {
        PrintGeometry::PrintMesh result=source;for(auto&vertex:result.vertices)vertex=map(vertex);return result;
    }

    QMatrix4x4 matrix() const
    {
        QMatrix4x4 result;for(int row=0;row<3;++row)for(int column=0;column<3;++column)result(row,column)=float(m_matrix[std::size_t(row*3+column)]);return result;
    }

    QString summary() const
    {
        if(isIdentity())return QStringLiteral("Nominal");
        const auto axis=[this](int column){static constexpr const char*names[]={"X","Y","Z"};for(int row=0;row<3;++row){const int value=m_matrix[std::size_t(row*3+column)];if(value)return QStringLiteral("%1%2").arg(value>0?QStringLiteral("+"):QStringLiteral("-"),QString::fromLatin1(names[row]));}return QStringLiteral("?");};
        return QStringLiteral("X→%1  Y→%2  Z→%3").arg(axis(0),axis(1),axis(2));
    }

private:
    using Matrix=std::array<int,9>;
    static constexpr Matrix Identity{1,0,0,0,1,0,0,0,1};
    static Matrix multiply(const Matrix&left,const Matrix&right){Matrix result{};for(int row=0;row<3;++row)for(int column=0;column<3;++column)for(int index=0;index<3;++index)result[std::size_t(row*3+column)]+=left[std::size_t(row*3+index)]*right[std::size_t(index*3+column)];return result;}
    static Matrix rotationMatrix(Rotation rotation){switch(rotation){case Rotation::XPositive:return{1,0,0,0,0,-1,0,1,0};case Rotation::XNegative:return{1,0,0,0,0,1,0,-1,0};case Rotation::YPositive:return{0,0,1,0,1,0,-1,0,0};case Rotation::YNegative:return{0,0,-1,0,1,0,1,0,0};case Rotation::ZPositive:return{0,-1,0,1,0,0,0,0,1};case Rotation::ZNegative:return{0,1,0,-1,0,0,0,0,1};}return Identity;}
    Matrix m_matrix=Identity;
};
