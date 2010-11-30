/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkCaveSynchronizedRenderers.h"

#include "vtkCamera.h"
#include "vtkMath.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVServerOptions.h"
#include "vtkPerspectiveTransform.h"
#include "vtkProcessModule.h"
#include "vtkRenderer.h"
#include "vtkTimerLog.h"
#include "vtkTransform.h"
#include "vtkUnsignedCharArray.h"

#include "assert.h"


vtkStandardNewMacro(vtkCaveSynchronizedRenderers);
//----------------------------------------------------------------------------
vtkCaveSynchronizedRenderers::vtkCaveSynchronizedRenderers()
{
  this->NumberOfDisplays = 0;
  this->Displays= 0;
  this->SetNumberOfDisplays(1);

  this->DisplayOrigin[0] = 0.0;
  this->DisplayOrigin[1] = 0.0;
  this->DisplayOrigin[2] = 0.0;
  this->DisplayOrigin[3] = 1.0;
  this->DisplayX[0] = 0.0;
  this->DisplayX[1] = 0.0;
  this->DisplayX[2] = 0.0;
  this->DisplayX[3] = 1.0;
  this->DisplayY[0] = 0.0;
  this->DisplayY[1] = 0.0;
  this->DisplayY[2] = 0.0;
  this->DisplayY[3] = 1.0;

  this->SetParallelController(vtkMultiProcessController::GetGlobalController());

  // Initilize using pvx file specified on the command line options.
  vtkPVServerOptions* options = vtkPVServerOptions::SafeDownCast(
    vtkProcessModule::GetProcessModule()->GetOptions());
  if (!options)
    {
    vtkErrorMacro("Are you sure vtkCaveSynchronizedRenderers is crated on "
      "an appropriate processes?");
    }
  else
    {
    this->SetNumberOfDisplays(options->GetNumberOfMachines());
    for (int cc=0; cc < this->NumberOfDisplays; cc++)
      {
      this->DefineDisplay(cc, options->GetLowerLeft(cc),
        options->GetLowerRight(cc), options->GetUpperLeft(cc));
      }
    }
}

//----------------------------------------------------------------------------
vtkCaveSynchronizedRenderers::~vtkCaveSynchronizedRenderers()
{
  this->SetNumberOfDisplays(0);
}

//----------------------------------------------------------------------------
void vtkCaveSynchronizedRenderers::HandleStartRender()
{
  this->ImageReductionFactor = 1;
  this->Superclass::HandleStartRender();
  this->ComputeCamera(this->GetRenderer()->GetActiveCamera());
  this->GetRenderer()->ResetCameraClippingRange();
}

//-----------------------------------------------------------------------------
void vtkCaveSynchronizedRenderers::SetNumberOfDisplays(int numberOfDisplays)
{
  if (numberOfDisplays == this->NumberOfDisplays)
    {
    return;
    }
  double** newDisplays = 0;
  if (numberOfDisplays > 0)
    {
    newDisplays = new double*[numberOfDisplays];
    for (int i = 0; i < numberOfDisplays; ++i)
      {
      newDisplays[i] = new double[12];
      if (i < this->NumberOfDisplays)
        {
        memcpy(newDisplays[i], this->Displays[i], 12 * sizeof(double));
        }
      else
        {
        newDisplays[i][0] = -1.;
        newDisplays[i][1] = -1.;
        newDisplays[i][2] = -1.;
        newDisplays[i][3] = 1.0;

        newDisplays[i][4] = 1.0;
        newDisplays[i][5] = -1.0;
        newDisplays[i][6] = -1.0;
        newDisplays[i][7] = 1.0;

        newDisplays[i][8] = -1.0;
        newDisplays[i][9] = 1.0;
        newDisplays[i][10] = -1.0;
        newDisplays[i][11] = 1.0;
        }
      }
    }
  for (int i = 0; i < this->NumberOfDisplays; ++i)
    {
    delete [] this->Displays[i];
    }
  delete [] this->Displays;
  this->Displays = newDisplays;

  this->NumberOfDisplays = numberOfDisplays;
  this->Modified();
}

//-------------------------------------------------------------------------
void vtkCaveSynchronizedRenderers::DefineDisplay(
  int idx, double origin[3], double x[3], double y[3])
{
  if (idx >= this->NumberOfDisplays)
    {
    vtkErrorMacro("idx is too high !");
    return;
    }
  memcpy(&this->Displays[idx][0], origin, 3 * sizeof(double));
  memcpy(&this->Displays[idx][4], x, 3 * sizeof(double));
  memcpy(&this->Displays[idx][8], y, 3 * sizeof(double));
  if (idx == this->GetParallelController()->GetLocalProcessId())
    {
    memcpy(this->DisplayOrigin, origin, 3 * sizeof(double));
    memcpy(this->DisplayX, x, 3 * sizeof(double));
    memcpy(this->DisplayY, y, 3 * sizeof(double));
    }
  this->Modified();
}

//-------------------------------------------------------------------------
// Room camera is a camera in room coordinates that points at the display.
// Client camera is the camera on the client.  The out camera is the
// combination of the two used for the final cave display.
// It is the room camera transformed by the world camera.
void vtkCaveSynchronizedRenderers::ComputeCamera(vtkCamera* cam)
{
  int idx;
  // pos is the user position
  double pos[4];
  //cam->GetPosition(pos);
  pos[0] = 0.;
  pos[1] = 0.;
  pos[2] = 0.;
  pos[3] = 1.;

  // Use the camera here  tempoarily to get the client view transform.
 // Create a transform from the client camera.
  vtkTransform* trans = cam->GetViewTransformObject();
  // The displays are defined in camera coordinates.
  // We want to convert them to world coordinates.
  trans->Inverse();

  // Apply the transform on the local display info.
  double p[4];
  double o[4];
  double x[4];
  double y[4];
  trans->MultiplyPoint(pos, p);
  trans->MultiplyPoint(this->DisplayOrigin, o);
  trans->MultiplyPoint(this->DisplayX, x);
  trans->MultiplyPoint(this->DisplayY, y);
  // Handle homogeneous coordinates.
  for (idx = 0; idx < 3; ++idx)
    {
    p[idx] = p[idx] / p[3];
    o[idx] = o[idx] / o[3];
    x[idx] = x[idx] / x[3];
    y[idx] = y[idx] / y[3];
    }

  // Now compute the camera.
  float vn[3];
  float ox[3];
  float oy[3];
  float cp[3];
  float center[3];
  float offset[3];
  float xOffset, yOffset;
  float dist;
  float height;
  float width;
  float viewAngle;
  float tmp;

  // Compute the view plane normal.
  for ( idx = 0; idx < 3; ++idx)
    {
    ox[idx] = x[idx] - o[idx];
    oy[idx] = y[idx] - o[idx];
    center[idx] = o[idx] + 0.5*(ox[idx] + oy[idx]);
    cp[idx] = p[idx] - center[idx];
    }
  vtkMath::Cross(ox, oy, vn);
  vtkMath::Normalize(vn);
  // Compute distance to plane.
  dist = vtkMath::Dot(vn,cp);
  // Compute width and height of the window.
  width = sqrt(ox[0]*ox[0] + ox[1]*ox[1] + ox[2]*ox[2]);
  height = sqrt(oy[0]*oy[0] + oy[1]*oy[1] + oy[2]*oy[2]);

  // Point the camera orthogonal toward the plane.
  cam->SetPosition(p[0], p[1], p[2]);
  cam->SetFocalPoint(p[0]-vn[0], p[1]-vn[1], p[2]-vn[2]);
  cam->SetViewUp(oy[0], oy[1], oy[2]);

  // Compute view angle.
  viewAngle = atan(height/(2.0*dist)) * 360.0 / 3.1415926;
  cam->SetViewAngle(viewAngle);

  // Compute the shear/offset vector (focal point to window center).
  offset[0] = center[0] - (p[0]-dist*vn[0]);
  offset[1] = center[1] - (p[1]-dist*vn[1]);
  offset[2] = center[2] - (p[2]-dist*vn[2]);

  // Compute the normalized x and y components of shear offset.
  tmp = sqrt(ox[0]*ox[0] + ox[1]*ox[1] + ox[2]*ox[2]);
  xOffset = vtkMath::Dot(offset, ox) / (tmp * tmp);
  tmp = sqrt(oy[0]*oy[0] + oy[1]*oy[1] + oy[2]*oy[2]);
  yOffset = vtkMath::Dot(offset, oy) / (tmp * tmp);

  // Off angle positioning of window.
  cam->SetWindowCenter(2*xOffset, 2*yOffset);
}

//----------------------------------------------------------------------------
void vtkCaveSynchronizedRenderers::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}