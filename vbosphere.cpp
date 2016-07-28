#include "vbosphere.h"

#include <cstdio>
#include <cmath>

VBOSphere::~VBOSphere()
{
    delete [] v;
    delete [] n;
    delete [] tex;
    delete [] el;
}

VBOSphere::VBOSphere(float rad, int sl, int st) :
       radius(rad), slices(sl), stacks(st) 
{
  
    nVerts = (slices+1) * (stacks + 1);
    nFaces = (slices * 2 * (stacks-1) ) * 3;

    // Verts
    v = new float[3 * nVerts];
    // Normals
    n = new float[3 * nVerts];
    // Tex coords
    tex = new float[2 * nVerts];
    // Elements
    el = new unsigned int[nFaces];

    // Generate the vertex data
    generateVerts(v, n, tex, el);

    /*
    // Create and populate the buffer objects
    unsigned int handle[4];
    glGenBuffers(4, handle);

    glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * nVerts) * sizeof(float), v, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * nVerts) * sizeof(float), n, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, handle[2]);
    glBufferData(GL_ARRAY_BUFFER, (2 * nVerts) * sizeof(float), tex, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(unsigned int), el, GL_STATIC_DRAW);

    delete [] v;
    delete [] n;
    delete [] el;
    delete [] tex;

    // Create the VAO
    glGenVertexArrays( 1, &vaoHandle );
    glBindVertexArray(vaoHandle);

    glEnableVertexAttribArray(0);  // Vertex position
    glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
    glVertexAttribPointer( (GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0 );

    glEnableVertexAttribArray(1);  // Vertex normal
    glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
    glVertexAttribPointer( (GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, handle[2]);
    glEnableVertexAttribArray(2);  // Texture coords
    glVertexAttribPointer( (GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[3]);

    glBindVertexArray(0);
    */
}

/*
void VBOSphere::render() const {
    glBindVertexArray(vaoHandle);
    glDrawElements(GL_TRIANGLES, elements, GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));
}
*/

void VBOSphere::generateVerts(float * verts, float * norms, float * tex,
                             unsigned int * el)
{
	// Generate positions and normals    
    float theta, phi;
    float thetaFac = TwoPI / slices;
    float phiFac = M_PI / stacks;
    float nx, ny, nz, s, t;
    int   idx = 0, tIdx = 0;
    for( int i = 0; i <= slices; i++ ) {
		theta = i * thetaFac;
                s = (float)i / slices;
        for( int j = 0; j <= stacks; j++ ) {
			phi = j * phiFac;
                        t = (float)j / stacks;
			nx = sinf(phi) * cosf(theta);
			ny = sinf(phi) * sinf(theta);
			nz = cosf(phi);
			verts[idx] = radius * nx; verts[idx+1] = radius * ny; verts[idx+2] = radius * nz;
			norms[idx] = nx; norms[idx+1] = ny; norms[idx+2] = nz;
			idx += 3;

            tex[tIdx] = s;
            tex[tIdx+1] = t;
            tIdx += 2;
		}
	}

	// Generate the element list
	idx = 0;
    for( int i = 0; i < slices; i++ ) {
        int stackStart = i * (stacks + 1);
        int nextStackStart = (i+1) * (stacks+1);
        for( int j = 0; j < stacks; j++ ) {
			if( j == 0 ) {
				el[idx] = stackStart;
				el[idx+1] = stackStart + 1;
				el[idx+2] = nextStackStart + 1;
				idx += 3;
			} else if( j == stacks - 1) {
				el[idx] = stackStart + j;
				el[idx+1] = stackStart + j + 1;
				el[idx+2] = nextStackStart + j;
				idx += 3;
			} else {
				el[idx] = stackStart + j;
				el[idx+1] = stackStart + j + 1;
				el[idx+2] = nextStackStart + j + 1;
				el[idx+3] = nextStackStart + j;
				el[idx+4] = stackStart + j;
				el[idx+5] = nextStackStart + j + 1;
				idx += 6;
			}
		}
	}
}

int VBOSphere::getVertexArrayHandle() {
	return this->vaoHandle;
}

float *VBOSphere::getv()
{
    return v;
}

unsigned int VBOSphere::getnVerts()
{
    return nVerts;
}

float *VBOSphere::getn()
{
    return n;
}

float *VBOSphere::gettc()
{
    return tex;
}

unsigned int *VBOSphere::getelems()
{
    return el;
}

unsigned int VBOSphere::getnFaces()
{
    return nFaces;
}

