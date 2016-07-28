#ifndef VBOSPHERE_H
#define VBOSPHERE_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ToRadian(x) ((x) * M_PI / 180.0f)
#define ToDegree(x) ((x) * 180.0f / M_PI)
#define TwoPI (float)(2 * M_PI)

class VBOSphere
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

    unsigned int vaoHandle;
    //GLuint nVerts, elements;
	float radius;
    int slices, stacks;

    void generateVerts(float * , float * ,float *, unsigned int *);

public:
    VBOSphere(float, int, int);
    ~VBOSphere();

    //void render() const;

    int getVertexArrayHandle();

    float *getv();
    unsigned int getnVerts();
    float *getn();
    float *gettc();
    unsigned int *getelems();
    unsigned int  getnFaces();
};

#endif // VBOSPHERE_H
