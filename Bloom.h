#include <QWindow>
#include <QTimer>
#include <QString>
#include <QKeyEvent>

#include <QVector3D>
#include <QMatrix4x4>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_3_Core>

#include <QOpenGLShaderProgram>

#include "teapot.h"
#include "vboplane.h"
#include "vbosphere.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ToRadian(x) ((x) * M_PI / 180.0f)
#define ToDegree(x) ((x) * 180.0f / M_PI)
#define TwoPI (float)(2 * M_PI)

//class MyWindow : public QWindow, protected QOpenGLFunctions_3_3_Core
class MyWindow : public QWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit MyWindow();
    ~MyWindow();
    virtual void keyPressEvent( QKeyEvent *keyEvent );    

private slots:
    void render();

private:    
    void initialize();
    void modCurTime();

    void initShaders();
    void CreateVertexBuffer();    
    void initMatrices();
    void setupFBO();

    void pass1();
    void pass2();    

    void PrepareTexture(GLenum TextureTarget, const QString& FileName, GLuint& TexObject, bool flip);
    float computeLogAveLuminance();

protected:
    void resizeEvent(QResizeEvent *);

private:
    QOpenGLContext *mContext;
    QOpenGLFunctions_4_3_Core *mFuncs;

    QOpenGLShaderProgram *mProgram;

    QTimer mRepaintTimer;
    double currentTimeMs;
    double currentTimeS;
    bool   mUpdateSize;
    float  tPrev, angle;

    bool   displayMode = true; // with (true) or without effect (false)

    GLuint mVAOTeapot, mVAOPlane, mVAOSphere, mVAOFSQuad, mVBO, mIBO, mFBOHandle, intermediateFBO;
    GLuint mPositionBufferHandle, mColorBufferHandle;
    GLuint mRotationMatrixLocation;

    GLuint pass1Index, pass2Index;
    GLuint renderTex, intermediateTex;

    Teapot    *mTeapot;
    VBOPlane  *mPlane;
    VBOSphere *mSphere;

    QMatrix4x4 ModelMatrixTeapot, ModelMatrixSphere, ViewMatrix, ProjectionMatrix;
    QMatrix4x4 ModelMatrixBackPlane, ModelMatrixBotPlane, ModelMatrixTopPlane;

    //debug
    void printMatrix(const QMatrix4x4& mat);
};
