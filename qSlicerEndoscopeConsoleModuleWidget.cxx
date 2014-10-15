/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// Qt includes
#include <QDebug>

// SlicerQt includes
#include "qSlicerEndoscopeConsoleModuleWidget.h"
#include "ui_qSlicerEndoscopeConsoleModuleWidget.h"

// SlicerQt includes
#include "qSlicerApplication.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerWidget.h"

// QVTK includes
#include <QVTKWidget.h>

// qMRMLWidget includes
#include "qMRMLThreeDView.h"
#include "qMRMLThreeDWidget.h"
#include "qMRMLSliderWidget.h"

// Qt includes
#include <QApplication>
#include <QTimer>

// VTK includes
#include <vtkNew.h>
#include "vtkMRMLScalarVolumeDisplayNode.h"
#include "vtkMRMLSliceNode.h"


//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerEndoscopeConsoleModuleWidgetPrivate: public Ui_qSlicerEndoscopeConsoleModuleWidget
{
public:
  qSlicerEndoscopeConsoleModuleWidgetPrivate();
};

//-----------------------------------------------------------------------------
// qSlicerEndoscopeConsoleModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerEndoscopeConsoleModuleWidgetPrivate::qSlicerEndoscopeConsoleModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerEndoscopeConsoleModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerEndoscopeConsoleModuleWidget::qSlicerEndoscopeConsoleModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerEndoscopeConsoleModuleWidgetPrivate )
{
  this->ImageCaptureTimer = new QTimer(this);
  this->ImageCaptureFlag = 0;

  this->CV_VideoCaptureIF = NULL;
  this->CV_ImageSize = cvSize(0,0);
  this->CV_ImageCaptured = NULL;

  this->VTK_ImageCaptured = NULL;
  this->VTK_BackgroundRenderer = NULL;
  this->VTK_BackgroundActor = NULL;

  this->VideoChannel = 0;
  this->VideoFlipped = 0;
  this->VideoRefreshInterval = 0;
}

//-----------------------------------------------------------------------------
qSlicerEndoscopeConsoleModuleWidget::~qSlicerEndoscopeConsoleModuleWidget()
{
  if (this->VTK_ImageCaptured)
    {
    this->VTK_ImageCaptured->Delete();
    }

  if (this->VTK_BackgroundActor)
    {
    this->VTK_BackgroundActor->Delete();
    }

  if (this->VTK_BackgroundRenderer)
    {
    this->VTK_BackgroundRenderer->Delete();
    }
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::setup()
{
  Q_D(qSlicerEndoscopeConsoleModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();
    
  connect(d->VideoON, SIGNAL(toggled(bool)),
          this, SLOT(onVideoONToggled(bool)));
    
  connect(d->VideoImageFlipON, SIGNAL(toggled(bool)),
          this, SLOT(onVideoImageFlipONToggled(bool)));

  connect(d->VideoChannelSpinBox, SIGNAL(valueChanged(int)),
          this, SLOT(onVideoChannelValueChanged(int)));

  connect(d->VideoChannelSpinBox, SIGNAL(valueChanged(int)),
          d->VideoChannelSlider, SLOT(setValue(int)));

  connect(d->VideoChannelSlider, SIGNAL(valueChanged(int)),
          d->VideoChannelSpinBox, SLOT(setValue(int)));

  connect(d->VideoRefreshIntervalSpinBox, SIGNAL(valueChanged(int)),
          this, SLOT(onVideoRefreshIntervalChanged(int)));
    
  connect(d->VideoRefreshIntervalSpinBox, SIGNAL(valueChanged(int)),
          d->VideoRefreshIntervalSlider, SLOT(setValue(int)));
    
  connect(d->VideoRefreshIntervalSlider, SIGNAL(valueChanged(int)),
          d->VideoRefreshIntervalSpinBox, SLOT(setValue(int)));
    
  d->VideoChannelSpinBox->setRange(0,10);
  d->VideoChannelSlider->setRange(0,10);
  d->VideoChannelSpinBox->setValue(0);
    
  d->VideoRefreshIntervalSpinBox->setRange(1,1000);
  d->VideoRefreshIntervalSlider->setRange(1,1000);
  d->VideoRefreshIntervalSpinBox->setValue(50);
    
  // QTimer setup
  if (this->ImageCaptureTimer)
    {
    this->ImageCaptureFlag = 0;
    connect(this->ImageCaptureTimer, SIGNAL(timeout()),
	    this, SLOT(timerIntrupt()));
    }

  this->VideoFlipped = d->VideoImageFlipON->isChecked() ? 1 : 0;
  this->VideoChannel = d->VideoChannelSpinBox->value();
  this->VideoRefreshInterval = d->VideoRefreshIntervalSpinBox->value();
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::timerIntrupt()
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    if (this->ImageCaptureFlag == 1)
      {
      this->CameraHandler();
      }
    
}

//-----------------------------------------------------------------------------
int qSlicerEndoscopeConsoleModuleWidget::CameraHandler()
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
 
    qSlicerApplication *  app = qSlicerApplication::application();
    vtkRenderer* activeRenderer = app->layoutManager()->activeThreeDRenderer();
    activeRenderer->SetLayer(1);

    IplImage* capturedImageTmp = NULL;
    IplImage* flippedImage = NULL;
    CvSize   newImageSize = cvSize(0,0);

    if (this->CV_VideoCaptureIF)
      {
      if ((capturedImageTmp = cvQueryFrame( this->CV_VideoCaptureIF )) == NULL)
        {
	std::cerr << "Couldn't acquire image" << std::endl;
	return 0;
        }

        newImageSize = cvGetSize( capturedImageTmp );
        
        // check if the image size is changed
        if (newImageSize.width != this->CV_ImageSize.width ||
            newImageSize.height != this->CV_ImageSize.height)
        {
            this->CV_ImageSize.width = newImageSize.width;
            this->CV_ImageSize.height = newImageSize.height;
	    this->CV_ImageCaptured = cvCreateImage(this->CV_ImageSize, IPL_DEPTH_8U, capturedImageTmp->nChannels);
            flippedImage = cvCreateImage(this->CV_ImageSize, IPL_DEPTH_8U, capturedImageTmp->nChannels);
            
	    if (this->VTK_ImageCaptured)
	      {
	      this->VTK_ImageCaptured->SetDimensions(newImageSize.width, newImageSize.height, 1);
	      this->VTK_ImageCaptured->SetExtent(0, newImageSize.width-1, 0, newImageSize.height-1, 0, 0 );
#if VTK_MAJOR_VERSION <= 5
	      this->VTK_ImageCaptured->SetNumberOfScalarComponents(capturedImageTmp->nChannels);
	      this->VTK_ImageCaptured->SetScalarTypeToUnsignedChar();
	      this->VTK_ImageCaptured->AllocateScalars();
	      this->VTK_ImageCaptured->Update();
#else
	      this->VTK_ImageCaptured->AllocateScalars(VTK_UNSIGNED_CHAR, capturedImageTmp->nChannels);
	      this->VTK_ImageCaptured->Modified();
#endif
	      }
        }

        // Display video image
        if (this->VideoFlipped == 1)
	  {
	  cvFlip(capturedImageTmp, capturedImageTmp, 0);
	  }
	cvCvtColor(capturedImageTmp, this->CV_ImageCaptured, CV_BGR2RGB);
        
        int imSize = this->CV_ImageSize.width*this->CV_ImageSize.height*capturedImageTmp->nChannels;
        memcpy((void*)this->VTK_ImageCaptured->GetScalarPointer(), (void*)this->CV_ImageCaptured->imageData, imSize);

        if (this->VTK_ImageCaptured && this->VTK_BackgroundRenderer)
	  {
	  this->VTK_ImageCaptured->Modified();
	  this->ViewerBackgroundOn(activeRenderer);
	  this->VTK_BackgroundRenderer->GetRenderWindow()->Render();
	  }
      }

    return 1;
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::onVideoONToggled(bool checked)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);

    if (checked)
      {
      // QTimer start
      if (this->ImageCaptureTimer->isActive())
	{
	this->ImageCaptureTimer->stop();
	}
      this->ImageCaptureFlag = 0;

      this->StartCamera(this->VideoChannel, NULL);

      this->ImageCaptureTimer->start(this->VideoRefreshInterval);
      }
    else
      {
      // QTimer stop
      if (this->ImageCaptureTimer->isActive())
	{
	this->ImageCaptureTimer->stop();
	}
      this->ImageCaptureFlag = 0;

      this->StopCamera();

      if (this->CV_VideoCaptureIF)
        {
	cvReleaseCapture(&this->CV_VideoCaptureIF);
        }
      }
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::onVideoImageFlipONToggled(bool checked)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    this->VideoFlipped = (checked ? 1 : 0);
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::onVideoChannelValueChanged(int channel)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    this->VideoChannel = channel;
}

//-----------------------------------------------------------------------------
void qSlicerEndoscopeConsoleModuleWidget::onVideoRefreshIntervalChanged(int interval)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);

    if (this->ImageCaptureTimer->isActive())
      {
      this->ImageCaptureTimer->stop();
      }
    
    this->VideoRefreshInterval = interval;

    this->ImageCaptureTimer->start(this->VideoRefreshInterval);
}

//-----------------------------------------------------------------------------
int qSlicerEndoscopeConsoleModuleWidget::StartCamera(int channel, const char* path)
{
    // if channel = -1, OpenCV will read image from the video file specified by path
    
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    // video import setup
    // Start reading images
    if (channel < 0 && path != NULL)
      {
      this->CV_VideoCaptureIF = cvCaptureFromAVI(path);
      }
    else
      {
      this->CV_VideoCaptureIF = cvCaptureFromCAM(channel);
      }

    if (!this->CV_VideoCaptureIF)
      {
      return 0;
      }

    qSlicerApplication *  app = qSlicerApplication::application();
    vtkRenderer* activeRenderer = app->layoutManager()->activeThreeDRenderer();
    activeRenderer->SetLayer(1);

    this->ImageCaptureFlag = 1;

    if (!this->VTK_ImageCaptured)
      {
      this->VTK_ImageCaptured = vtkImageData::New();
      this->VTK_ImageCaptured->SetDimensions(64,64,1);
      this->VTK_ImageCaptured->SetExtent(0,63,0,63,0,0);
      this->VTK_ImageCaptured->SetSpacing(1,1,1);
      this->VTK_ImageCaptured->SetOrigin(0,0,0);
#if VTK_MAJOR_VERSION <= 5
      this->VTK_ImageCaptured->SetNumberOfScalarComponents(3);
      this->VTK_ImageCaptured->SetScalarTypeToUnsignedChar();
      this->VTK_ImageCaptured->AllocateScalars();
#else
      this->VTK_ImageCaptured->AllocateScalars(VTK_UNSIGNED_CHAR,3);
#endif
      this->VTK_ImageCaptured->Modified();
      }

    this->ViewerBackgroundOn(activeRenderer);

    return 1;
}

//-----------------------------------------------------------------------------
int qSlicerEndoscopeConsoleModuleWidget::StopCamera()
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    // QTimer stop
    if (this->CV_VideoCaptureIF)
      {
        if (this->ImageCaptureTimer->isActive())
	  {
	  this->ImageCaptureTimer->stop();
	  }

        cvReleaseCapture(&this->CV_VideoCaptureIF);

        qSlicerApplication *  app = qSlicerApplication::application();
        vtkRenderer* activeRenderer = app->layoutManager()->activeThreeDRenderer();
        activeRenderer->SetLayer(1);

        this->ViewerBackgroundOff(activeRenderer);

	this->CV_VideoCaptureIF = NULL;
	this->CV_ImageCaptured = NULL;
	this->CV_ImageSize = cvSize(0,0);
      }

    return 1;
}

//-----------------------------------------------------------------------------
int qSlicerEndoscopeConsoleModuleWidget::ViewerBackgroundOn(vtkRenderer* activeRenderer)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);
    
    vtkRenderWindow* activeRenderWindow = activeRenderer->GetRenderWindow();

    if (activeRenderer && this->VTK_ImageCaptured)
      {
      if (!this->VTK_BackgroundActor)
	{
        // VTK Renderer
        this->VTK_BackgroundActor = vtkImageActor::New();
	}

#if VTK_MAJOR_VERSION <= 5
      this->VTK_BackgroundActor->SetInput(this->VTK_ImageCaptured);
#else
      this->VTK_BackgroundActor->SetInputData(this->VTK_ImageCaptured);
#endif

      if (!this->VTK_BackgroundRenderer)
	{
        this->VTK_BackgroundRenderer = vtkRenderer::New();
        this->VTK_BackgroundRenderer->InteractiveOff();
        this->VTK_BackgroundRenderer->SetLayer(0);
        this->VTK_BackgroundRenderer->GradientBackgroundOff();
        this->VTK_BackgroundRenderer->SetBackground(0,0,0);

	activeRenderWindow->AddRenderer(this->VTK_BackgroundRenderer);
        }

      this->VTK_BackgroundRenderer->AddActor(this->VTK_BackgroundActor);
      this->VTK_BackgroundActor->Modified();

      // Adjust camera position so that image covers the draw area.
      vtkCamera* camera = this->VTK_BackgroundRenderer->GetActiveCamera();
      camera->ParallelProjectionOn();

      // Set up the background camera to fill the renderer with the image
      double origin[3];
      double spacing[3];
      int extent[6];

      this->VTK_ImageCaptured->GetOrigin( origin );
      this->VTK_ImageCaptured->GetSpacing( spacing );
      this->VTK_ImageCaptured->GetExtent( extent );

      double xc = origin[0] + 0.5*(extent[0] + extent[1])*spacing[0];
      double yc = origin[1] + 0.5*(extent[2] + extent[3])*spacing[1];
      double yd = (extent[3] - extent[2] + 1)*spacing[1];
      double d = camera->GetDistance();

      camera->SetParallelScale(0.5*yd);
      camera->SetFocalPoint(xc,yc,0.0);
      camera->SetPosition(xc,yc,d);

      activeRenderWindow->Render();
    }
    
    return 0;
}

//-----------------------------------------------------------------------------
int qSlicerEndoscopeConsoleModuleWidget::ViewerBackgroundOff(vtkRenderer* activeRenderer)
{
    Q_D(qSlicerEndoscopeConsoleModuleWidget);

    if (activeRenderer)
    {
        // Slicer default background color
        this->VTK_BackgroundRenderer->GradientBackgroundOn();
        this->VTK_BackgroundRenderer->SetBackground(0.7568627450980392, 0.7647058823529412, 0.9098039215686275);
        this->VTK_BackgroundRenderer->SetBackground2(0.4549019607843137, 0.4705882352941176, 0.7450980392156863);

        this->VTK_BackgroundRenderer->RemoveActor(this->VTK_BackgroundActor);
        this->VTK_BackgroundRenderer->GetRenderWindow()->Render();
    }
    
    return 0;
}
