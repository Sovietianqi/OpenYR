#pragma once

#include "Core/Definitions.h"
#include "Math/Point3D.h"

#include <cmath>
#include <cstring>

//========================================================================
// Matrix3D
//
// A 4x3 (effectively 4x4) affine transformation matrix for 3D graphics.
// Stored as 3 rows of 4 floats each (row-major order).
//
// The matrix is used for:
//   - Model/world transformations
//   - View/camera transformations
//   - Voxel rendering transforms
//   - Projection calculations
//
// Layout: float Data[12] = { row0_x, row0_y, row0_z, row0_w,
//                              row1_x, row1_y, row1_z, row1_w,
//                              row2_x, row2_y, row2_z, row2_w }
//
// The last row (0,0,0,1) is implicit for affine transforms.
//========================================================================

class Matrix3D
{
public:
    //========================================================================
    // Construction
    //========================================================================

    Matrix3D() noexcept
    {
        std::memset(Data, 0, sizeof(Data));
    }

    // Plain float constructor (row-major order)
    Matrix3D(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23) noexcept
    {
        row[0][0] = m00; row[0][1] = m01; row[0][2] = m02; row[0][3] = m03;
        row[1][0] = m10; row[1][1] = m11; row[1][2] = m12; row[1][3] = m13;
        row[2][0] = m20; row[2][1] = m21; row[2][2] = m22; row[2][3] = m23;
    }

    // Column vector constructor
    Matrix3D(
        const Point3D& x,
        const Point3D& y,
        const Point3D& z,
        const Point3D& pos) noexcept
    {
        row[0][0] = x.X; row[0][1] = y.X; row[0][2] = z.X; row[0][3] = pos.X;
        row[1][0] = x.Y; row[1][1] = y.Y; row[1][2] = z.Y; row[1][3] = pos.Y;
        row[2][0] = x.Z; row[2][1] = y.Z; row[2][2] = z.Z; row[2][3] = pos.Z;
    }

    // Copy constructor
    Matrix3D(const Matrix3D& other) noexcept
    {
        std::memcpy(Data, other.Data, sizeof(Data));
    }

    Matrix3D(Matrix3D&& other) noexcept
    {
        std::memcpy(Data, other.Data, sizeof(Data));
    }

    //========================================================================
    // Assignment
    //========================================================================

    Matrix3D& operator=(const Matrix3D& other) noexcept
    {
        if (this != &other)
            std::memcpy(Data, other.Data, sizeof(Data));
        return *this;
    }

    Matrix3D& operator=(Matrix3D&& other) noexcept
    {
        std::memcpy(Data, other.Data, sizeof(Data));
        return *this;
    }

    //========================================================================
    // Matrix multiplication
    //========================================================================

    Matrix3D operator*(const Matrix3D& B) const noexcept
    {
        Matrix3D ret;

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                ret.row[i][j] =
                    this->row[i][0] * B.row[0][j] +
                    this->row[i][1] * B.row[1][j] +
                    this->row[i][2] * B.row[2][j];
            }

            ret.row[i][3] =
                this->row[i][0] * B.row[0][3] +
                this->row[i][1] * B.row[1][3] +
                this->row[i][2] * B.row[2][3] +
                this->row[i][3];
        }

        return ret;
    }

    Matrix3D& operator*=(const Matrix3D& other) noexcept
    {
        *this = *this * other;
        return *this;
    }

    //========================================================================
    // Vector transformation
    //========================================================================

    // Transform a point (applies both rotation and translation)
    Point3D TransformPoint(const Point3D& point) const noexcept
    {
        return Point3D(
            row[0][0] * point.X + row[0][1] * point.Y + row[0][2] * point.Z + row[0][3],
            row[1][0] * point.X + row[1][1] * point.Y + row[1][2] * point.Z + row[1][3],
            row[2][0] * point.X + row[2][1] * point.Y + row[2][2] * point.Z + row[2][3]
        );
    }

    // Transform a vector (rotation only, no translation)
    Point3D TransformVector(const Point3D& vec) const noexcept
    {
        return Point3D(
            row[0][0] * vec.X + row[0][1] * vec.Y + row[0][2] * vec.Z,
            row[1][0] * vec.X + row[1][1] * vec.Y + row[1][2] * vec.Z,
            row[2][0] * vec.X + row[2][1] * vec.Y + row[2][2] * vec.Z
        );
    }

    Point3D operator*(const Point3D& point) const noexcept
    {
        return TransformPoint(point);
    }

    //========================================================================
    // Identity matrix
    //========================================================================

    static Matrix3D Identity() noexcept
    {
        Matrix3D mtx;
        mtx.MakeIdentity();
        return mtx;
    }

    void MakeIdentity() noexcept
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                row[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    //========================================================================
    // Translation
    //========================================================================

    void Translate(float x, float y, float z) noexcept
    {
        TranslateX(x);
        TranslateY(y);
        TranslateZ(z);
    }

    void Translate(const Point3D& vec) noexcept
    {
        Translate(vec.X, vec.Y, vec.Z);
    }

    void TranslateX(float x) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][3] += x * row[i][0];
    }

    void TranslateY(float y) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][3] += y * row[i][1];
    }

    void TranslateZ(float z) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][3] += z * row[i][2];
    }

    static Matrix3D CreateTranslation(float x, float y, float z) noexcept
    {
        Matrix3D mtx = Identity();
        mtx.row[0][3] = x;
        mtx.row[1][3] = y;
        mtx.row[2][3] = z;
        return mtx;
    }

    //========================================================================
    // Scale
    //========================================================================

    void Scale(float factor) noexcept
    {
        for (float& f : Data)
            f *= factor;
    }

    void Scale(float x, float y, float z) noexcept
    {
        ScaleX(x);
        ScaleY(y);
        ScaleZ(z);
    }

    void ScaleX(float factor) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][0] *= factor;
    }

    void ScaleY(float factor) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][1] *= factor;
    }

    void ScaleZ(float factor) noexcept
    {
        for (int i = 0; i < 3; ++i)
            row[i][2] *= factor;
    }

    static Matrix3D CreateScale(float x, float y, float z) noexcept
    {
        Matrix3D mtx;
        mtx.row[0][0] = x;
        mtx.row[1][1] = y;
        mtx.row[2][2] = z;
        return mtx;
    }

    static Matrix3D CreateScale(float factor) noexcept
    {
        return CreateScale(factor, factor, factor);
    }

    //========================================================================
    // Rotation
    //========================================================================

    void RotateX(float theta) noexcept
    {
        float s = std::sin(theta);
        float c = std::cos(theta);

        for (int i = 0; i < 3; ++i)
        {
            float y = row[i][1];
            float z = row[i][2];
            row[i][1] = y * c - z * s;
            row[i][2] = y * s + z * c;
        }
    }

    void RotateY(float theta) noexcept
    {
        float s = std::sin(theta);
        float c = std::cos(theta);

        for (int i = 0; i < 3; ++i)
        {
            float x = row[i][0];
            float z = row[i][2];
            row[i][0] = x * c + z * s;
            row[i][2] = -x * s + z * c;
        }
    }

    void RotateZ(float theta) noexcept
    {
        float s = std::sin(theta);
        float c = std::cos(theta);

        for (int i = 0; i < 3; ++i)
        {
            float x = row[i][0];
            float y = row[i][1];
            row[i][0] = x * c - y * s;
            row[i][1] = x * s + y * c;
        }
    }

    static Matrix3D CreateRotationX(float theta) noexcept
    {
        Matrix3D mtx = Identity();
        float s = std::sin(theta);
        float c = std::cos(theta);
        mtx.row[1][1] = c;  mtx.row[1][2] = -s;
        mtx.row[2][1] = s;  mtx.row[2][2] =  c;
        return mtx;
    }

    static Matrix3D CreateRotationY(float theta) noexcept
    {
        Matrix3D mtx = Identity();
        float s = std::sin(theta);
        float c = std::cos(theta);
        mtx.row[0][0] =  c;  mtx.row[0][2] = s;
        mtx.row[2][0] = -s;  mtx.row[2][2] = c;
        return mtx;
    }

    static Matrix3D CreateRotationZ(float theta) noexcept
    {
        Matrix3D mtx = Identity();
        float s = std::sin(theta);
        float c = std::cos(theta);
        mtx.row[0][0] = c;  mtx.row[0][1] = -s;
        mtx.row[1][0] = s;  mtx.row[1][1] =  c;
        return mtx;
    }

    //========================================================================
    // Transpose
    //========================================================================

    void Transpose() noexcept
    {
        *this = Transposed();
    }

    Matrix3D Transposed() const noexcept
    {
        Matrix3D result;

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                result.row[i][j] = row[j][i];
            }
        }

        // The translation column in the transposed matrix
        // For a proper affine transform transpose, we need:
        result.row[0][3] = -(row[0][0] * row[0][3] + row[1][0] * row[1][3] + row[2][0] * row[2][3]);
        result.row[1][3] = -(row[0][1] * row[0][3] + row[1][1] * row[1][3] + row[2][1] * row[2][3]);
        result.row[2][3] = -(row[0][2] * row[0][3] + row[1][2] * row[1][3] + row[2][2] * row[2][3]);

        return result;
    }

    //========================================================================
    // Accessors
    //========================================================================

    float GetXVal() const noexcept { return row[0][3]; }
    float GetYVal() const noexcept { return row[1][3]; }
    float GetZVal() const noexcept { return row[2][3]; }

    Point3D GetTranslation() const noexcept
    {
        return Point3D(row[0][3], row[1][3], row[2][3]);
    }

    void SetTranslation(const Point3D& pos) noexcept
    {
        row[0][3] = pos.X;
        row[1][3] = pos.Y;
        row[2][3] = pos.Z;
    }

    void SetTranslation(float x, float y, float z) noexcept
    {
        row[0][3] = x;
        row[1][3] = y;
        row[2][3] = z;
    }

    //========================================================================
    // Data members
    //========================================================================

    union
    {
        float row[3][4];
        float Data[12];
    };

    static const Matrix3D IdentityMatrix;
};

inline const Matrix3D Matrix3D::IdentityMatrix = Matrix3D::Identity();

//========================================================================
// Validate binary layout
//========================================================================

static_assert(sizeof(Matrix3D) == sizeof(float) * 12,
    "Matrix3D must be exactly 12 float values (3x4 matrix)");