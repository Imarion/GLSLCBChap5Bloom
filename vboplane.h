#ifndef VBOPLANE_H
#define VBOPLANE_H

class VBOPlane
{
private:
    unsigned int nFaces;

    // Vertices
    float *v;
    unsigned int nVerts;

    // Normals
    float *n;

    // Tex coords
    float *tex;

    // Elements
    unsigned int *el;


public:
    ~VBOPlane();
    VBOPlane(float, float, int, int, float smax = 1.0f, float tmax = 1.0f);

    float *getv();
    unsigned int getnVerts();
    float *getn();
    float *gettc();
    unsigned int *getelems();
    unsigned int  getnFaces();
};

#endif // VBOPLANE_H
