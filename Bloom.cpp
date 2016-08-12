#include "Bloom.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <cmath>
#include <cstring>
#include <sstream>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), tPrev(0), angle(M_PI / 2.0f), sigma2(25.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    bloomBufWidth  = this->width()/8;
    bloomBufHeight = this->height()/8;

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    CreateVertexBuffer();
    initShaders();
    pass1Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass1");
    pass2Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass2");
    pass3Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass3");
    pass4Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass4");
    pass5Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass5");

    initMatrices();
    setupFBO();
    setupSamplers();
    computeBlurWeights();

    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
}

void MyWindow::CreateVertexBuffer()
{
    // *** Teapot
    mFuncs->glGenVertexArrays(1, &mVAOTeapot);
    mFuncs->glBindVertexArray(mVAOTeapot);

    QMatrix4x4 transform;
    //transform.translate(QVector3D(0.0f, 1.5f, 0.25f));
    mTeapot = new Teapot(14, transform);

    // Create and populate the buffer objects
    unsigned int TeapotHandles[4];
    glGenBuffers(4, TeapotHandles);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[2]);
    glBufferData(GL_ARRAY_BUFFER, (2 * mTeapot->getnVerts()) * sizeof(float), mTeapot->gettc(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mTeapot->getnFaces() * sizeof(unsigned int), mTeapot->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, TeapotHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, TeapotHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // vertex texture coord
    mFuncs->glBindVertexBuffer(2, TeapotHandles[2], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[3]);

    mFuncs->glBindVertexArray(0);

    // *** Plane
    mFuncs->glGenVertexArrays(1, &mVAOPlane);
    mFuncs->glBindVertexArray(mVAOPlane);

    mPlane = new VBOPlane(20.0f, 10.0f, 1.0, 1.0);

    // Create and populate the buffer objects
    unsigned int PlaneHandles[4];
    glGenBuffers(4, PlaneHandles);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[2]);
    glBufferData(GL_ARRAY_BUFFER, (2 * mPlane->getnVerts()) * sizeof(float), mPlane->gettc(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mPlane->getnFaces() * sizeof(unsigned int), mPlane->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, PlaneHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, PlaneHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // vertex texture coord
    mFuncs->glBindVertexBuffer(2, PlaneHandles[2], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[3]);

    mFuncs->glBindVertexArray(0);

    // *** Sphere
    mFuncs->glGenVertexArrays(1, &mVAOSphere);
    mFuncs->glBindVertexArray(mVAOSphere);

    mSphere = new VBOSphere(2.0f, 50, 50);

    // Create and populate the buffer objects
    unsigned int SphereHandles[4];
    glGenBuffers(4, SphereHandles);

    glBindBuffer(GL_ARRAY_BUFFER, SphereHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mSphere->getnVerts()) * sizeof(float), mSphere->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, SphereHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mSphere->getnVerts()) * sizeof(float), mSphere->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[2]);
    glBufferData(GL_ARRAY_BUFFER, (2 * mPlane->getnVerts()) * sizeof(float), mSphere->gettc(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, SphereHandles[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSphere->getnFaces() * sizeof(unsigned int), mSphere->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, SphereHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, SphereHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // vertex texture coord
    mFuncs->glBindVertexBuffer(2, SphereHandles[2], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, SphereHandles[3]);


    // *** Array for full-screen quad
    GLfloat verts[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
    };
    GLfloat tc[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
    };

    // Set up the buffers

    unsigned int fsqhandle[2];
    glGenBuffers(2, fsqhandle);

    glBindBuffer(GL_ARRAY_BUFFER, fsqhandle[0]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(float), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, fsqhandle[1]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), tc, GL_STATIC_DRAW);

    // Set up the VAO
    mFuncs->glGenVertexArrays( 1, &mVAOFSQuad );
    mFuncs->glBindVertexArray(mVAOFSQuad);

    // Vertex positions
    mFuncs->glBindVertexBuffer(0, fsqhandle[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex texture coordinates
    mFuncs->glBindVertexBuffer(1, fsqhandle[1], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 1);

    mFuncs->glBindVertexArray(0);

}

void MyWindow::initMatrices()
{
    ModelMatrixTeapot.translate( 3.0f, -5.0f, 1.5f);
    ModelMatrixTeapot.rotate( -90.0f, QVector3D(1.0f, 0.0f, 0.0f));

    ModelMatrixSphere.translate( -3.0f, -3.0f, 2.0f);

    ModelMatrixBackPlane.rotate(90.0f, QVector3D(1.0f, 0.0f, 0.0f));
    ModelMatrixBotPlane.translate(0.0f, -5.0f, 0.0f);
    ModelMatrixTopPlane.translate(0.0f,  5.0f, 0.0f);
    ModelMatrixTopPlane.rotate(180.0f, 1.0f, 0.0f, 0.0f);

    //ModelMatrixPlane.translate(0.0f, -0.45f, 0.0f);

    ViewMatrix.lookAt(QVector3D(2.0f, 0.0f, 14.0f), QVector3D(0.0f,0.0f,0.0f), QVector3D(0.0f,1.0f,0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    ProjectionMatrix.perspective(60.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0;
    EvolvingVal += 0.1f;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    pass1();
    pass2();
    pass3();
    pass4();
    pass5();

    mContext->swapBuffers(this);
}

void MyWindow::pass1()
{   
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

    glClearColor(0.5f,0.5f,0.5f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // *** Draw teapot
    mFuncs->glBindVertexArray(mVAOTeapot);       

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    QVector4D worldLightl = QVector4D(0.0f-7.0f, 4.0f, 2.5f, 1.0f);
    QVector4D worldLightm = QVector4D(0.0f, 4.0f, 2.5f, 1.0f);
    QVector4D worldLightr = QVector4D(0.0f+7.0f, 4.0f, 2.5f, 1.0f);
    QVector3D intense     = QVector3D(0.6f, 0.6f, 0.6f);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Lights[0].Position", ViewMatrix * worldLightl);
        mProgram->setUniformValue("Lights[1].Position", ViewMatrix * worldLightm);
        mProgram->setUniformValue("Lights[2].Position", ViewMatrix * worldLightr);

        mProgram->setUniformValue("Lights[0].Intensity", intense );
        mProgram->setUniformValue("Lights[1].Intensity", intense );
        mProgram->setUniformValue("Lights[2].Intensity", intense );

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.4f, 0.4f, 0.9f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixTeapot;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        glDrawElements(GL_TRIANGLES, 6 * mTeapot->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw planes
    mFuncs->glBindVertexArray(mVAOPlane);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Lights[0].Position", ViewMatrix * worldLightl);
        mProgram->setUniformValue("Lights[1].Position", ViewMatrix * worldLightm);
        mProgram->setUniformValue("Lights[2].Position", ViewMatrix * worldLightr);

        mProgram->setUniformValue("Lights[0].Intensity", intense );
        mProgram->setUniformValue("Lights[1].Intensity", intense );
        mProgram->setUniformValue("Lights[2].Intensity", intense );

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.9f, 0.3f, 0.2f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        // back plane
        QMatrix4x4 mvback = ViewMatrix * ModelMatrixBackPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvback);
        mProgram->setUniformValue("NormalMatrix", mvback.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvback);
        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        // Top plane
        QMatrix4x4 mvtop = ViewMatrix * ModelMatrixTopPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvtop);
        mProgram->setUniformValue("NormalMatrix", mvtop.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvtop);
        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        // Bot plane
        QMatrix4x4 mvbot = ViewMatrix * ModelMatrixBotPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvbot);
        mProgram->setUniformValue("NormalMatrix", mvbot.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvbot);

        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw sphere
    mFuncs->glBindVertexArray(mVAOSphere);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Lights[0].Position", ViewMatrix * worldLightl );
        mProgram->setUniformValue("Lights[1].Position", ViewMatrix * worldLightm );
        mProgram->setUniformValue("Lights[2].Position", ViewMatrix * worldLightr);

        mProgram->setUniformValue("Lights[0].Intensity", intense );
        mProgram->setUniformValue("Lights[1].Intensity", intense );
        mProgram->setUniformValue("Lights[2].Intensity", intense );

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.4f, 0.9f, 0.4f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixSphere;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);                

        glDrawElements(GL_TRIANGLES, mSphere->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();
}

void MyWindow::pass2()
{    

    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo);

    // We're writing to tex1 this time
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    glViewport(0, 0, bloomBufWidth, bloomBufHeight);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass2Index);

        mProgram->setUniformValue("LumThresh", 1.7f);

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();
}

void MyWindow::pass3()
{
    // We're writing to tex2 this time
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass3Index);

        for (int i = 0; i < 10; i++ ) {
            std::stringstream uniName;
            uniName << "Weight[" << i << "]";
            mProgram->setUniformValue(uniName.str().c_str(), weights[i]);
        }

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();
}

void MyWindow::pass4()
{

    // We're writing to tex1 this time
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass4Index);

        for (int i = 0; i < 10; i++ ) {
            std::stringstream uniName;
            uniName << "Weight[" << i << "]";
            mProgram->setUniformValue(uniName.str().c_str(), weights[i]);
        }

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();
}


void MyWindow::pass5()
{
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    //glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, this->width(), this->height());

    // In this pass, we're reading from tex1 (unit 1) and we want
    // linear sampling to get an extra blur
    mFuncs->glBindSampler(1, linearSampler);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass5Index);

        mProgram->setUniformValue( "AveLum",    computeLogAveLuminance() );
        mProgram->setUniformValue( "DoToneMap", displayMode );

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();

    // Revert to nearest sampling
    mFuncs->glBindSampler(1, nearestSampler);
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader fShader(QOpenGLShader::Fragment);    
    QFile         shaderFile;
    QByteArray    shaderSource;

    //Simple ADS
    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag   compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&fShader);
    qDebug() << "shader link: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureTarget, const QString& FileName, GLuint& TexObject, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture";
    if (flip==true) TexImg=TexImg.mirrored();

    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    glTexImage2D(TextureTarget, 0, GL_RGB, TexImg.width(), TexImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameterf(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_O:
            displayMode = !displayMode;
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_D:
            break;
        case Qt::Key_A:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}

void MyWindow::setupFBO() {
    // Generate and bind the framebuffer
    glGenFramebuffers(1, &hdrFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

    // Create the texture object    
    glGenTextures(1, &hdrTex);
    glActiveTexture(GL_TEXTURE0);  // Use texture unit 0
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, this->width(), this->height());

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrTex, 0);

    // Create the depth buffer
    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    mFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, this->width(), this->height());

    // Bind the depth buffer to the FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthBuf);

    // Set the targets for the fragment output variables
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    mFuncs->glDrawBuffers(1, drawBuffers);

    GLenum error = mFuncs->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (error != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "hdrfbo incomplete " << error;
        switch (error) {
            case GL_FRAMEBUFFER_UNDEFINED: // is returned if the specified framebuffer is the default read or draw framebuffer, but the default framebuffer does not exist.
                qDebug() << "GL_FRAMEBUFFER_UNDEFINED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: // is returned if any of the framebuffer attachment points are framebuffer incomplete.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: // is returned if the framebuffer does not have at least one image attached to it.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:  // is returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for any color attachment point(s) named by GL_DRAW_BUFFERi.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: // is returned if GL_READ_BUFFER is not GL_NONE and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment point named by GL_READ_BUFFER.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED: // is returned if the combination of internal formats of the attached images violates an implementation-dependent set of restrictions.
                qDebug() << "GL_FRAMEBUFFER_UNSUPPORTED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: // is returned if the value of GL_RENDERBUFFER_SAMPLES is not the same for all attached renderbuffers; if the value of GL_TEXTURE_SAMPLES is the not same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_RENDERBUFFER_SAMPLES does not match the value of GL_TEXTURE_SAMPLES.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; // is also returned if the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not the same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not GL_TRUE for all attached textures.
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            case GL_INVALID_ENUM: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_INVALID_ENUM";
                break;
            case GL_INVALID_OPERATION: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_INVALID_OPERATION";
                break;
        }
    }

    // Create an FBO for the bright-pass filter and blur
    glGenFramebuffers(1, &blurFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo);

    // Create two texture objects to ping-pong for the bright-pass filter
    // and the two-pass blur
    glGenTextures(1, &tex1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufWidth, bloomBufHeight);

    glGenTextures(1, &tex2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufWidth, bloomBufHeight);

    // Bind tex1 to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    error = mFuncs->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (error != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "blurfbo incomplete " << error;
        switch (error) {
            case GL_FRAMEBUFFER_UNDEFINED: // is returned if the specified framebuffer is the default read or draw framebuffer, but the default framebuffer does not exist.
                qDebug() << "GL_FRAMEBUFFER_UNDEFINED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: // is returned if any of the framebuffer attachment points are framebuffer incomplete.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: // is returned if the framebuffer does not have at least one image attached to it.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:  // is returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for any color attachment point(s) named by GL_DRAW_BUFFERi.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: // is returned if GL_READ_BUFFER is not GL_NONE and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment point named by GL_READ_BUFFER.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED: // is returned if the combination of internal formats of the attached images violates an implementation-dependent set of restrictions.
                qDebug() << "GL_FRAMEBUFFER_UNSUPPORTED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: // is returned if the value of GL_RENDERBUFFER_SAMPLES is not the same for all attached renderbuffers; if the value of GL_TEXTURE_SAMPLES is the not same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_RENDERBUFFER_SAMPLES does not match the value of GL_TEXTURE_SAMPLES.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; // is also returned if the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not the same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not GL_TRUE for all attached textures.
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            case GL_INVALID_ENUM: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_INVALID_ENUM";
                break;
            case GL_INVALID_OPERATION: // is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.
                qDebug() << "GL_INVALID_OPERATION";
                break;
        }
    }


    mFuncs->glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MyWindow::setupSamplers()
{
    // Set up two sampler objects for linear and nearest filtering
    GLuint samplers[2];
    mFuncs->glGenSamplers(2, samplers);
    linearSampler  = samplers[0];
    nearestSampler = samplers[1];

    GLfloat border[] = {0.0f,0.0f,0.0f,0.0f};
    // Set up the nearest sampler
    mFuncs->glSamplerParameteri(nearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    mFuncs->glSamplerParameteri(nearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    mFuncs->glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    mFuncs->glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    mFuncs->glSamplerParameterfv(nearestSampler, GL_TEXTURE_BORDER_COLOR, border);

    // Set up the linear sampler
    mFuncs->glSamplerParameteri(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mFuncs->glSamplerParameteri(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    mFuncs->glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    mFuncs->glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    mFuncs->glSamplerParameterfv(linearSampler, GL_TEXTURE_BORDER_COLOR, border);

    // We want nearest sampling except for the last pass.
    mFuncs->glBindSampler(0, nearestSampler);
    mFuncs->glBindSampler(1, nearestSampler);
    mFuncs->glBindSampler(2, nearestSampler);
}

float MyWindow::computeLogAveLuminance()
{
    float *texData = new float[this->width()*this->height()*3];
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, texData);
    float sum = 0.0f;
    for( int i = 0; i < this->width() * this->height(); i++ )
    {
        float lum = QVector3D::dotProduct(QVector3D(texData[i*3+0], texData[i*3+1], texData[i*3+2]), QVector3D(0.2126f, 0.7152f, 0.0722f) );
        sum += logf( lum + 0.00001f );
    }

    //prog.setUniform( "AveLum", expf( sum / (width*height) ) );
    //printf("(%f)\n", exp( sum / (width*height) ) );
    delete [] texData;

    //qDebug() << "Ave luminance: " << expf(sum / (this->width()*this->height()));

    return expf(sum / (this->width()*this->height()));
}

void MyWindow::computeBlurWeights()
{
    float sum;

    // Compute and sum the weights
    weights[0] = gauss(0, sigma2);
    sum = weights[0];
    for( int i = 1; i < 10; i++ ) {
        weights[i] = gauss(float(i), sigma2);
        sum += 2 * weights[i];
    }

    // Normalize the weights and set the uniform
    for( int i = 0; i < 10; i++ ) {
        weights[i] /= sum;
    }
}

float MyWindow::gauss(float x, float sigma2 )
{
    double coeff = 1.0 / (TwoPI * sigma2);
    double expon = -(x * x) / (2.0 * sigma2);

    return (float) (coeff * std::exp(expon));
}
