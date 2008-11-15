/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.

   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    GLRenderer.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/

#include "GLRenderer.h"
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>

using namespace std;

GLRenderer::GLRenderer(MasterController* pMasterController) : 
  AbstrRenderer(pMasterController),
  m_p1DTransTex(NULL),
  m_p2DTransTex(NULL),
  m_p1DData(NULL),
  m_p2DData(NULL),
  m_pFBO3DImageLast(NULL),
  m_pFBO3DImageCurrent(NULL),
  m_iFilledBuffers(0),
  m_pProgramTrans(NULL),
  m_pProgram1DTransSlice(NULL),
  m_pProgram2DTransSlice(NULL),
  m_LogoTex(NULL)
{
}

GLRenderer::~GLRenderer() {
}

bool GLRenderer::Initialize() {
  if (!AbstrRenderer::Initialize()) {
    m_pMasterController->DebugOut()->Error("GLRenderer::Initialize","Error in parent call -> aborting");
    return false;
  }

  string strPotential1DTransName = SysTools::ChangeExt(m_pDataset->Filename(), "1dt");
  string strPotential2DTransName = SysTools::ChangeExt(m_pDataset->Filename(), "2dt");
  
  if (SysTools::FileExists(strPotential1DTransName)) 
    m_pMasterController->MemMan()->Get1DTransFromFile(strPotential1DTransName, this, &m_p1DTrans, &m_p1DTransTex);
  else
    m_pMasterController->MemMan()->GetEmpty1DTrans(m_pDataset->Get1DHistogram()->GetFilledSize(), this, &m_p1DTrans, &m_p1DTransTex);

  if (SysTools::FileExists(strPotential2DTransName)) 
    m_pMasterController->MemMan()->Get2DTransFromFile(strPotential2DTransName, this, &m_p2DTrans, &m_p2DTransTex);
  else
    m_pMasterController->MemMan()->GetEmpty2DTrans(m_pDataset->Get2DHistogram()->GetFilledSize(), this, &m_p2DTrans, &m_p2DTransTex);

  m_pProgramTrans = m_pMasterController->MemMan()->GetGLSLProgram(SysTools::GetFromResourceOnMac("false_Transfer-VS.glsl"),
                                                                SysTools::GetFromResourceOnMac("Transfer-FS.glsl"));

  m_pProgram1DTransSlice = m_pMasterController->MemMan()->GetGLSLProgram(SysTools::GetFromResourceOnMac("Transfer-VS.glsl"),
                                                                SysTools::GetFromResourceOnMac("1D-slice-FS.glsl"));

  m_pProgram2DTransSlice = m_pMasterController->MemMan()->GetGLSLProgram(SysTools::GetFromResourceOnMac("Transfer-VS.glsl"),
                                                                SysTools::GetFromResourceOnMac("2D-slice-FS.glsl"));

  if (m_pProgramTrans == NULL || m_pProgram1DTransSlice == NULL || m_pProgram2DTransSlice == NULL ||
      !m_pProgramTrans->IsValid() || !m_pProgram1DTransSlice->IsValid() || !m_pProgram2DTransSlice->IsValid()) {
    m_pMasterController->DebugOut()->Error("GLRenderer::Initialize","Error loading transfer shaders.");
    return false;
  } else {
    m_pProgramTrans->Enable();
    m_pProgramTrans->SetUniformVector("texColor",0);
    m_pProgramTrans->SetUniformVector("texDepth",1);
    m_pProgramTrans->Disable();

    m_pProgram1DTransSlice->Enable();
    m_pProgram1DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram1DTransSlice->SetUniformVector("texTrans1D",1);
    m_pProgram1DTransSlice->Disable();

    m_pProgram2DTransSlice->Enable();
    m_pProgram2DTransSlice->SetUniformVector("texVolume",0);
    m_pProgram2DTransSlice->SetUniformVector("texTrans2D",1);
    m_pProgram2DTransSlice->Disable();
  }

  return true;
}

void GLRenderer::Changed1DTrans() {
  m_p1DTrans->GetByteArray(&m_p1DData);
  m_p1DTransTex->SetData(m_p1DData);

  AbstrRenderer::Changed1DTrans();
}

void GLRenderer::Changed2DTrans() {
  m_p2DTrans->GetByteArray(&m_p2DData);
  m_p2DTransTex->SetData(m_p2DData);

  AbstrRenderer::Changed2DTrans();
}

bool GLRenderer::SetBackgroundColors(FLOATVECTOR3 vColors[2]) {
  if (AbstrRenderer::SetBackgroundColors(vColors)) {
    glClearColor(m_vBackgroundColors[0].x,m_vBackgroundColors[0].y,m_vBackgroundColors[0].z,0);
    return true;
  } else return false;
}

void GLRenderer::Resize(const UINTVECTOR2& vWinSize) {
  AbstrRenderer::Resize(vWinSize);
  m_pMasterController->DebugOut()->Message("GLRenderer::Resize","Resizing to %i x %i", vWinSize.x, vWinSize.y);
  CreateOffscreenBuffers();
}

void GLRenderer::RenderSeperatingLines() {
    // set render area to fullscreen
    SetRenderTargetAreaScissor(RA_FULLSCREEN);
    SetRenderTargetArea(RA_FULLSCREEN);
    glDisable( GL_SCISSOR_TEST );

    // render seperating lines
    glDisable(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, 1, -1, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_LINES);
      glColor4f(1.0f,1.0f,1.0f,1.0f);
      glVertex3f(0,-1,0);
      glVertex3f(0,1,0);
      glVertex3f(-1,0,0);
      glVertex3f(1,0,0);
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Paint() {
  // if we are redrawing clear the framebuffer (if requested)
  if (m_bPerformRedraw && m_bClearFramebuffer) glClear(GL_DEPTH_BUFFER_BIT);

  bool bNewDataToShow;
  if (m_eViewMode == VM_SINGLE) {
    // set render area to fullscreen
    SetRenderTargetArea(RA_FULLSCREEN);

    // bind offscreen buffer
    m_pFBO3DImageCurrent->Write();

    switch (m_eFullWindowMode) {
       case WM_3D       : {
                              // plan the frame
                              Plan3DFrame();
                              // execute the frame
                              bNewDataToShow = Execute3DFrame(RA_FULLSCREEN); 
                              break;
                          }
       case WM_SAGITTAL : 
       case WM_AXIAL    : 
       case WM_CORONAL  : bNewDataToShow = Render2DView(RA_FULLSCREEN, m_eFullWindowMode, m_piSlice[size_t(m_eFullWindowMode)]); break;
       default          : m_pMasterController->DebugOut()->Error("GLRenderer::Paint","Invalid Windowmode");
                          bNewDataToShow = false;
                          break;

    }

    // unbind offscreen buffer
    m_pFBO3DImageCurrent->FinishWrite();

  } else { // VM_TWOBYTWO 
    int iActiveRenderWindows = 0;
    int iReadyWindows = 0;
    for (unsigned int i = 0;i<4;i++) if (m_bRedrawMask[i]) iActiveRenderWindows++;

    // bind offscreen buffer
    m_pFBO3DImageCurrent->Write();

    for (unsigned int i = 0;i<4;i++) {
      ERenderArea eArea = ERenderArea(int(RA_TOPLEFT)+i);

      if (m_bRedrawMask[size_t(m_e2x2WindowMode[i])]) {
        SetRenderTargetArea(eArea);
        bool bLocalNewDataToShow;
        switch (m_e2x2WindowMode[i]) {
           case WM_3D       : {
                                // plan the frame
                                Plan3DFrame();
                                // execute the frame
                                bLocalNewDataToShow = Execute3DFrame(eArea);
                                // are we done traversing the LOD levels
                                m_bRedrawMask[size_t(m_e2x2WindowMode[i])] = (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) || (m_iCurrentLODOffset > m_iMinLODForCurrentView);
                                break;
                              }
           case WM_SAGITTAL : 
           case WM_AXIAL    : 
           case WM_CORONAL  : bLocalNewDataToShow= Render2DView(eArea, m_e2x2WindowMode[i], m_piSlice[size_t(m_e2x2WindowMode[i])]); 
                              m_bRedrawMask[size_t(m_e2x2WindowMode[i])] = false;
                              break;
           default          : m_pMasterController->DebugOut()->Error("GLRenderer::Paint","Invalid Windowmode");
                              bLocalNewDataToShow = false; 
                              break;
        }
        
        if (bLocalNewDataToShow) iReadyWindows++;
      } else {
        SetRenderTargetArea(RA_FULLSCREEN);
        SetRenderTargetAreaScissor(eArea);
        RerenderPreviousResult(false);
      }

      bNewDataToShow = (iActiveRenderWindows > 0) && (iReadyWindows==iActiveRenderWindows);
    }

    // render cross to seperate the four subwindows
    RenderSeperatingLines();

    // unbind offscreen buffer
    m_pFBO3DImageCurrent->FinishWrite();
  }
  // if the image is complete swap the offscreen buffers
  if (bNewDataToShow) swap(m_pFBO3DImageLast, m_pFBO3DImageCurrent);

  // show the result
  if (bNewDataToShow || m_iFilledBuffers < 2) RerenderPreviousResult(true);

  m_bPerformRedraw = false;
}


void GLRenderer::SetRenderTargetArea(ERenderArea eREnderArea) {
  switch (eREnderArea) {
    case RA_TOPLEFT     : SetViewPort(UINTVECTOR2(0,m_vWinSize.y/2), UINTVECTOR2(m_vWinSize.x/2,m_vWinSize.y)); break;
    case RA_TOPRIGHT    : SetViewPort(m_vWinSize/2, m_vWinSize); break;
    case RA_LOWERLEFT   : SetViewPort(UINTVECTOR2(0,0),m_vWinSize/2); break;
    case RA_LOWERRIGHT  : SetViewPort(UINTVECTOR2(m_vWinSize.x/2,0), UINTVECTOR2(m_vWinSize.x,m_vWinSize.y/2)); break;
    case RA_FULLSCREEN  : SetViewPort(UINTVECTOR2(0,0), m_vWinSize); break;
    default             : m_pMasterController->DebugOut()->Error("GLRenderer::SetRenderTargetArea","Invalid render area set"); break;
  }
}

void GLRenderer::SetRenderTargetAreaScissor(ERenderArea eREnderArea) {
  switch (eREnderArea) {
    case RA_TOPLEFT     : glScissor(0,m_vWinSize.y/2, m_vWinSize.x/2,m_vWinSize.y); break;
    case RA_TOPRIGHT    : glScissor(m_vWinSize.x/2, m_vWinSize.y/2, m_vWinSize.x, m_vWinSize.y); break;
    case RA_LOWERLEFT   : glScissor(0,0,m_vWinSize.x/2, m_vWinSize.y/2); break;
    case RA_LOWERRIGHT  : glScissor(m_vWinSize.x/2,0,m_vWinSize.x,m_vWinSize.y/2); break;
    case RA_FULLSCREEN  : glScissor(0,0,m_vWinSize.x, m_vWinSize.y); break;
    default             : m_pMasterController->DebugOut()->Error("GLRenderer::SetRenderTargetAreaScissor","Invalid render area set"); break;
  }
  glEnable( GL_SCISSOR_TEST );
}

void GLRenderer::SetViewPort(UINTVECTOR2 viLowerLeft, UINTVECTOR2 viUpperRight) {

  UINTVECTOR2 viSize = viUpperRight-viLowerLeft;

  float aspect=(float)viSize.x/(float)viSize.y;
  float fovy = 50.0f;
  float nearPlane = 0.1f;
  float farPlane = 100.0f;
	glViewport(viLowerLeft.x,viLowerLeft.y,viSize.x,viSize.y);
	glMatrixMode(GL_PROJECTION);		
	glLoadIdentity();
	gluPerspective(fovy,aspect,nearPlane,farPlane); 	// Set Projection. Arguments are FOV (in degrees), aspect-ratio, near-plane, far-plane.
  
  // forward the GL projection matrix to the culling object
  FLOATMATRIX4 mProjection;
  mProjection.getProjection();
  m_FrustumCullingLOD.SetProjectionMatrix(mProjection);
  m_FrustumCullingLOD.SetScreenParams(fovy,aspect,nearPlane,viSize.y);

	glMatrixMode(GL_MODELVIEW);
}


bool GLRenderer::Render2DView(ERenderArea eREnderArea, EWindowMode eDirection, UINT64 iSliceIndex) {
  // clear the depth buffer if instructed
  if (m_bClearFramebuffer) glClear(GL_DEPTH_BUFFER_BIT);  
   switch (m_eRenderMode) {
    case RM_2DTRANS    :  m_p2DTransTex->Bind(1); 
                          m_pProgram2DTransSlice->Enable();
                          break;
    default            :  m_p1DTransTex->Bind(1); 
                          m_pProgram1DTransSlice->Enable();
                          break;
  }

  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);

  UINT64 iCurrentLOD = 0;
  UINTVECTOR3 vVoxelCount;

  /// \todo change this code to use something better than the biggest single brick LOD level
  for (UINT64 i = 0;i<m_pDataset->GetInfo()->GetLODLevelCount();i++) {
    if (m_pDataset->GetInfo()->GetBrickCount(i).volume() == 1) {
        iCurrentLOD = i;
        vVoxelCount = UINTVECTOR3(m_pDataset->GetInfo()->GetDomainSize(i));
    }
  }

  SetBrickDepShaderVarsSlice(vVoxelCount);

  // convert 3D variables to the more general ND scheme used in the memory manager, e.i. convert 3-vectors to stl vectors
  vector<UINT64> vLOD; vLOD.push_back(iCurrentLOD);
  vector<UINT64> vBrick; 
  vBrick.push_back(0);vBrick.push_back(0);vBrick.push_back(0);

  // get the 3D texture from the memory manager
  GLTexture3D* t = m_pMasterController->MemMan()->Get3DTexture(m_pDataset, vLOD, vBrick, 0, m_iFrameCounter);
  if(t!=NULL) t->Bind(0);

  // clear the target at the beginning
  SetRenderTargetAreaScissor(eREnderArea);
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);

  FLOATVECTOR3 vMinCoords(0.5f/FLOATVECTOR3(vVoxelCount));
  FLOATVECTOR3 vMaxCoords(1.0f-vMinCoords);

  UINT64VECTOR3 vDomainSize = m_pDataset->GetInfo()->GetDomainSize();
  DOUBLEVECTOR3 vAspectRatio = m_pDataset->GetInfo()->GetScale() * DOUBLEVECTOR3(vDomainSize);  

  DOUBLEVECTOR2 vWinAspectRatio = 1.0 / DOUBLEVECTOR2(m_vWinSize);
  vWinAspectRatio = vWinAspectRatio / vWinAspectRatio.maxVal();

  switch (eDirection) {
    case WM_CORONAL : {
                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xz()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.y);
                          glBegin(GL_QUADS);
                            glTexCoord3d(vMinCoords.x,fSliceIndex,vMaxCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,fSliceIndex,vMaxCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,fSliceIndex,vMinCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMinCoords.x,fSliceIndex,vMinCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    case WM_AXIAL : {
                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.xy()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.z);
                          glBegin(GL_QUADS);
                            glTexCoord3d(vMinCoords.x,vMaxCoords.y,fSliceIndex);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,vMaxCoords.y,fSliceIndex);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMaxCoords.x,vMinCoords.y,fSliceIndex);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(vMinCoords.x,vMinCoords.y,fSliceIndex);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    case WM_SAGITTAL : {
                          DOUBLEVECTOR2 v2AspectRatio = vAspectRatio.yz()*DOUBLEVECTOR2(vWinAspectRatio);
                          v2AspectRatio = v2AspectRatio / v2AspectRatio.maxVal();
                          double fSliceIndex = double(iSliceIndex)/double(vDomainSize.x);
                          glBegin(GL_QUADS);
                            glTexCoord3d(fSliceIndex,vMinCoords.y,vMaxCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMaxCoords.y,vMaxCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, +1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMaxCoords.y,vMinCoords.z);
                            glVertex3d(+1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                            glTexCoord3d(fSliceIndex,vMinCoords.y,vMinCoords.z);
                            glVertex3d(-1.0f*v2AspectRatio.x, -1.0f*v2AspectRatio.y, -0.5f);
                          glEnd();
                          break;
                      }
    default        :  m_pMasterController->DebugOut()->Error("GLRenderer::Render2DView","Invalid windowmode set"); break;
  }

  m_pMasterController->MemMan()->Release3DTexture(t);

  glEnable(GL_DEPTH_TEST);

  switch (m_eRenderMode) {
    case RM_2DTRANS    :  m_pProgram2DTransSlice->Disable(); break;
    default            :  m_pProgram1DTransSlice->Disable(); break;
  }

  return true;
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor) {
  UINT64VECTOR3 vDomainSize = m_pDataset->GetInfo()->GetDomainSize();
  UINT64 iMaxDomainSize = vDomainSize.maxVal();
  FLOATVECTOR3 vExtend = FLOATVECTOR3(vDomainSize)/float(iMaxDomainSize) * FLOATVECTOR3(m_pDataset->GetInfo()->GetScale());

  FLOATVECTOR3 vCenter(0,0,0);
  RenderBBox(vColor, vCenter, vExtend);
}

void GLRenderer::RenderBBox(const FLOATVECTOR4 vColor, const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend) {
  FLOATVECTOR3 vMinPoint, vMaxPoint;
  vMinPoint = (vCenter - vExtend/2.0);
  vMaxPoint = (vCenter + vExtend/2.0);

  glBegin(GL_LINES);
    glColor4f(vColor.x,vColor.y,vColor.z,vColor.w);
    // FRONT
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);

    // BACK
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);

    // CONNECTION
    glVertex3f(vMinPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f(vMinPoint.x, vMaxPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x,vMinPoint.y,vMinPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y, vMaxPoint.z);
    glVertex3f( vMaxPoint.x, vMaxPoint.y,vMinPoint.z);
  glEnd();

}

bool GLRenderer::CheckForRedraw() {
  if (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame || m_iCurrentLODOffset > m_iMinLODForCurrentView) {
    if (m_iCheckCounter == 0)  {
      m_pMasterController->DebugOut()->Message("GLRenderer::CheckForRedraw","Still drawing...");
      return true;
    } else m_iCheckCounter--;
  }
  return m_bPerformRedraw;
}


bool GLRenderer::Execute3DFrame(ERenderArea eREnderArea) {
  // are we starting a new LOD level?
  if (m_iBricksRenderedInThisSubFrame == 0) m_iFilledBuffers = 0;

  // if there is something left in the TODO list
  if (m_vCurrentBrickList.size() > m_iBricksRenderedInThisSubFrame) {
    // setup shaders vars
    SetDataDepShaderVars(); 

    // clear target at the beginning
    if (m_iBricksRenderedInThisSubFrame == 0) {
      SetRenderTargetAreaScissor(eREnderArea);
      glClearColor(0,0,0,0);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
      glDisable( GL_SCISSOR_TEST );
    }

    Render3DView();

    // if there is nothing left todo in this subframe -> present the result
    if (m_vCurrentBrickList.size() == m_iBricksRenderedInThisSubFrame) return true;
  } 
  return false;
}

void GLRenderer::RerenderPreviousResult(bool bTransferToFramebuffer) {
  // clear the framebuffer
  if (m_bClearFramebuffer) {
    glDepthMask(GL_FALSE);
    if (m_vBackgroundColors[0] == m_vBackgroundColors[1]) {
      glClearColor(m_vBackgroundColors[0].x,m_vBackgroundColors[0].y,m_vBackgroundColors[0].z,0);
      glClear(GL_COLOR_BUFFER_BIT); 
    } else DrawBackGradient();
    DrawLogo();
    glDepthMask(GL_TRUE);
  }

  if (bTransferToFramebuffer) {
    glViewport(0,0,m_vWinSize.x,m_vWinSize.y);
    m_iFilledBuffers++;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }


  m_pFBO3DImageLast->Read(GL_TEXTURE0);
  m_pFBO3DImageLast->ReadDepth(GL_TEXTURE1);

  m_pProgramTrans->Enable();

  glDisable(GL_DEPTH_TEST);

  glBegin(GL_QUADS);
    glColor4d(1,1,1,1);
    glTexCoord2d(0,1);
    glVertex3d(-1.0,  1.0, -0.5);
    glTexCoord2d(1,1);
    glVertex3d( 1.0,  1.0, -0.5);
    glTexCoord2d(1,0);
    glVertex3d( 1.0, -1.0, -0.5);
    glTexCoord2d(0,0);
    glVertex3d(-1.0, -1.0, -0.5);
  glEnd();

  m_pProgramTrans->Disable();

  m_pFBO3DImageLast->FinishRead();
  m_pFBO3DImageLast->FinishDepthRead();
  glEnable(GL_DEPTH_TEST);
}


void GLRenderer::DrawLogo() {
  // \todo add the whole logo handling and positioning code

  if (m_LogoTex == NULL) return;

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-0.5, +0.5, +0.5, -0.5, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  m_LogoTex->Bind();
  glDisable(GL_TEXTURE_3D);
  glEnable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
    glColor4d(1,1,1,1);
    glTexCoord2d(0,0);
    glVertex3d(0.2, 0.4, -0.5);
    glTexCoord2d(1,0);
    glVertex3d(0.4, 0.4, -0.5);
    glTexCoord2d(1,1);
    glVertex3d(0.4, 0.2, -0.5);
    glTexCoord2d(0,1);
    glVertex3d(0.2, 0.2, -0.5);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void GLRenderer::DrawBackGradient() {
  glDisable(GL_DEPTH_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-0.5, +0.5, +0.5, -0.5, 0.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_TEXTURE_3D);
  glDisable(GL_TEXTURE_2D);

  glBegin(GL_QUADS);
    glColor4d(m_vBackgroundColors[1].x,m_vBackgroundColors[1].y,m_vBackgroundColors[1].z,1);
    glVertex3d(-1.0,  1.0, -0.5);
    glVertex3d( 1.0,  1.0, -0.5);
    glColor4d(m_vBackgroundColors[0].x,m_vBackgroundColors[0].y,m_vBackgroundColors[0].z,1);
    glVertex3d( 1.0, -1.0, -0.5);
    glVertex3d(-1.0, -1.0, -0.5);
  glEnd();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glEnable(GL_DEPTH_TEST);
}

void GLRenderer::Cleanup() {
  m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgramTrans);
  m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram1DTransSlice);
  m_pMasterController->MemMan()->FreeGLSLProgram(m_pProgram2DTransSlice);
}


void GLRenderer::SetBrickDepShaderVarsSlice(const UINTVECTOR3& vVoxelCount) {
  if (m_eRenderMode ==  RM_2DTRANS) {
    FLOATVECTOR3 vStep = 1.0f/FLOATVECTOR3(vVoxelCount);
    m_pProgram2DTransSlice->SetUniformVector("vVoxelStepsize", vStep.x, vStep.y, vStep.z);
  }
}


const FLOATVECTOR2 GLRenderer::SetDataDepShaderVars() {
  size_t       iMaxValue = m_p1DTrans->GetSize();
  unsigned int iMaxRange = (unsigned int)(1<<m_pDataset->GetInfo()->GetBitwith());
  float fScale = float(iMaxRange)/float(iMaxValue);
  float fGradientScale = 1.0f/m_pDataset->GetMaxGradMagnitude();

  switch (m_eRenderMode) {
    case RM_1DTRANS    :  {
                            m_pProgram1DTransSlice->Enable();
                            m_pProgram1DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram1DTransSlice->Disable();
                            break;
                          }
    case RM_2DTRANS    :  {
                            m_pProgram2DTransSlice->Enable();
                            m_pProgram2DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram2DTransSlice->SetUniformVector("fGradientScale",fGradientScale);
                            m_pProgram2DTransSlice->Disable();
                            break;
                          }
    case RM_ISOSURFACE : {
                            m_pProgram2DTransSlice->Enable();
                            m_pProgram2DTransSlice->SetUniformVector("fTransScale",fScale);
                            m_pProgram2DTransSlice->SetUniformVector("fGradientScale",fGradientScale);
                            m_pProgram2DTransSlice->Disable();
                            break;
                          }
    case RM_INVALID    :  m_pMasterController->DebugOut()->Error("GLRenderer::SetDataDepShaderVars","Invalid rendermode set"); break;
  }


  return FLOATVECTOR2(fScale,fGradientScale);
}

void GLRenderer::CreateOffscreenBuffers() {
  if (m_pFBO3DImageLast != NULL) m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageLast);
  if (m_pFBO3DImageCurrent != NULL) m_pMasterController->MemMan()->FreeFBO(m_pFBO3DImageCurrent);

  if (m_vWinSize.area() > 0) {
    switch (m_eBlendPrecision) {
      case BP_8BIT  : m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA8, 8*4, true);
                      m_pFBO3DImageCurrent = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA8, 8*4, true);
                      break;
      case BP_16BIT : m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true);
                      m_pFBO3DImageCurrent = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA16F_ARB, 16*4, true);
                      break;
      case BP_32BIT : m_pFBO3DImageLast = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA32F_ARB, 32*4, true);
                      m_pFBO3DImageCurrent = m_pMasterController->MemMan()->GetFBO(GL_NEAREST, GL_NEAREST, GL_CLAMP, m_vWinSize.x, m_vWinSize.y, GL_RGBA32F_ARB, 32*4, true);
                      break;
      default       : m_pMasterController->DebugOut()->Message("GPUSBVR::CreateOffscreenBuffer","Invalid Blending Precision");
                      m_pFBO3DImageLast = NULL; m_pFBO3DImageCurrent = NULL;
                      break;
    }
  }
}

void GLRenderer::SetBlendPrecision(EBlendPrecision eBlendPrecision) {
  if (eBlendPrecision != m_eBlendPrecision) {
    AbstrRenderer::SetBlendPrecision(eBlendPrecision);
    CreateOffscreenBuffers();
  }
}
