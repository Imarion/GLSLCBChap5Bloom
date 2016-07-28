#ifndef VBOTEAPOT_H
#define VBOTEAPOT_H

#include <QMatrix4x4>
#include <QMatrix3x3>
#include <QVector3D>

class Teapot
{
private:
    int nFaces;

    // Vertices
    float *v;
    int nVerts;

    // Normals
    float *n;

    // Tex coords
    float *tc;

    // Elements
    unsigned int *elems;

    void generateVerts(float * , float * ,float *, unsigned int *, float , float);

    void generatePatches(float * in_v, float * in_n, float *in_tc, unsigned int* in_el, int grid);
    void buildPatchReflect(int patchNum,
                           float *B, float *dB,
                           float *v, float *n, float *, unsigned int *el,
                           int &index, int &elIndex, int &, int grid,
                           bool reflectX, bool reflectY);
    void buildPatch(QVector3D patch[][4],
                    float *B, float *dB,
                    float *in_v, float *in_n, float *in_tc, unsigned int *in_el,
                    int &index, int &elIndex, int &, int grid, QMatrix3x3 reflect, bool invertNormal);
    void getPatch( int patchNum, QVector3D patch[][4], bool reverseV );

    void computeBasisFunctions( float * B, float * dB, int grid );
    QVector3D evaluate( int gridU, int gridV, float *B, QVector3D patch[][4] );
    QVector3D evaluateNormal( int gridU, int gridV, float *B, float *dB, QVector3D patch[][4] );
    void moveLid(int,float *,const QMatrix4x4 &);
    QVector3D mattimesvec(QMatrix3x3, QVector3D);

public:
    ~Teapot();
    Teapot(int grid, const QMatrix4x4& lidTransform);

    float *getv();
    int    getnVerts();
    float *getn();
    float *gettc();
    unsigned int *getelems();

    int    getnFaces();
};

#endif // VBOTEAPOT_H
