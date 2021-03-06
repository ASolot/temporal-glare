#include <QGridLayout>
#include <QTimer>
#include <QShortcut>


#define _USE_MATH_DEFINES
#include <math.h>

#include "TGViewerWindow.h"


TGViewerWindow::TGViewerWindow()
{
	setWindowTitle(tr("Temporal Glare Renderer"));
	tgViewerWidget = new TGViewerWidget(&tgRenderer, this);

	// main layout creation
	QHBoxLayout *main_layout = new QHBoxLayout;
    main_layout->addWidget(tgViewerWidget,2);
	QVBoxLayout *controls_layout = new QVBoxLayout;
	main_layout->addLayout(controls_layout);
	
	// image loader layout 
	QVBoxLayout *image_io_layout = new QVBoxLayout;
    QLabel *imageLoadLabel = new QLabel(tr("Image I/O"));
	imageLoadLabel->setAlignment(Qt::AlignLeft);
	image_io_layout->addWidget(imageLoadLabel);
	QPushButton *load_exr_button = new QPushButton("Load .exr image", this);
	image_io_layout->addWidget(load_exr_button);
	QPushButton *save_png_button = new QPushButton("Save .png image", this);
	image_io_layout->addWidget(save_png_button);
	controls_layout->addLayout(image_io_layout);

	// exposure function layout 
	QVBoxLayout *exposure_layout = new QVBoxLayout;
	QLabel *exposureLabel = new QLabel(tr("Exposure mode"));
	exposureLabel->setAlignment(Qt::AlignLeft);
	exposure_layout->addWidget(exposureLabel);
	QStringList commands = {"Auto","Manual"};
	QComboBox *exposure_combo = new QComboBox(this);
	exposure_combo->addItems(commands);
	exposure_layout->addWidget(exposure_combo);

	QLabel *alphaLabel = new QLabel(tr("Alpha"));
	exposureLabel->setAlignment(Qt::AlignCenter);
	exposure_layout->addWidget(alphaLabel);

	alphaSB = new QDoubleSpinBox(this);
	alphaSB->setMinimum(-10);
	alphaSB->setMaximum(10);
	alphaSB->setSingleStep(0.1);
	exposure_layout->addWidget(alphaSB);

	controls_layout->addLayout(exposure_layout);

	// tonemap function layout
	QVBoxLayout *tonemap_layout = new QVBoxLayout;
	QLabel *tonemapLabel = new QLabel(tr("Tonemapping operator"));
	tonemapLabel->setAlignment(Qt::AlignLeft);
	tonemap_layout->addWidget(tonemapLabel);

	QLabel *tonemapOperatorLabel = new QLabel(tr("Reinhard Extended"));
	tonemapOperatorLabel->setAlignment(Qt::AlignRight);
	tonemap_layout->addWidget(tonemapOperatorLabel);

	// 1st control
	QHBoxLayout *tonemap_control1_layout = new QHBoxLayout;
	QLabel *tonemapControl1Label = new QLabel(tr("Gamma"));
	tonemapControl1Label->setAlignment(Qt::AlignRight);
	tonemap_control1_layout->addWidget(tonemapControl1Label);
	
	control1SB = new QDoubleSpinBox(this);
	control1SB->setMinimum(0);
	control1SB->setMaximum(10);
	control1SB->setSingleStep(0.1);
	tonemap_control1_layout->addWidget(control1SB);

	tonemap_layout->addLayout(tonemap_control1_layout);

	// 2nd control
	QHBoxLayout *tonemap_control2_layout = new QHBoxLayout;
	QLabel *tonemapControl2Label = new QLabel(tr("Lwhite"));
	tonemapControl2Label->setAlignment(Qt::AlignRight);
	tonemap_control2_layout->addWidget(tonemapControl2Label);
	
	control2SB = new QDoubleSpinBox(this);
	control2SB->setMinimum(0);
	control2SB->setMaximum(14);
	control2SB->setSingleStep(0.1);
	tonemap_control2_layout->addWidget(control2SB);

	tonemap_layout->addLayout(tonemap_control2_layout);

	controls_layout->addLayout(tonemap_layout);


	// temporal glare params layout 
	
	// Camera position label 
	cameraPosLabel = new QLabel();
    cameraPosLabel->setAlignment(Qt::AlignHCenter);
	controls_layout->addWidget(cameraPosLabel);

	QHBoxLayout *focal_layout = new QHBoxLayout;
	QLabel *focalLabel = new QLabel(tr("Number of points"));
	focalLabel->setAlignment(Qt::AlignRight);
	focal_layout->addWidget(focalLabel);
	focalSB = new QDoubleSpinBox(this);
	focalSB->setMinimum(0);
	focalSB->setMaximum(15000);
	focal_layout->addWidget(focalSB);
	controls_layout->addLayout(focal_layout);

	controls_layout->addStretch();

	QLabel *controlsHelpLabel = new QLabel(tr("Key controls not available\n"
			));


	controls_layout->addWidget( controlsHelpLabel );
	renderTimeLabel = new QLabel(this);
	controls_layout->addWidget( renderTimeLabel );

	QWidget *central_widget = new QWidget( this );
	central_widget->setLayout(main_layout);
	setCentralWidget(central_widget);

	QShortcut *quit_shortcut = new QShortcut(QKeySequence(tr("Ctrl+Q", "Quit")), this );
	connect(quit_shortcut, &QShortcut::activated, this, &TGViewerWindow::close);

    tgViewerWidget->setFocus();

	// TODO: add refresh function

	// QTimer *timer = new QTimer;
	// connect(timer, &QTimer::timeout, this, SLOT (tgViewerWidget->refresh()));
	// timer->start(50);

	
	connect(tgViewerWidget, &TGViewerWidget::focalChanged, focalSB, &QDoubleSpinBox::setValue);
	connect(tgViewerWidget, &TGViewerWidget::renderTimeUpdated, this, &TGViewerWindow::renderTimeUpdated);

	connect(focalSB, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), tgViewerWidget, &TGViewerWidget::setFocal);
	
	connect(alphaSB,    static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), tgViewerWidget, &TGViewerWidget::setAlpha);
	connect(control1SB, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), tgViewerWidget, &TGViewerWidget::setGamma);
	connect(control2SB, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), tgViewerWidget, &TGViewerWidget::setLWhite);
	connect(exposure_combo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), tgViewerWidget, &TGViewerWidget::setExposureMode);

	// connect I/O pushbuttons
	connect(load_exr_button, SIGNAL (released()), this, SLOT (loadExrFile()));

	// Update labels
	// tgViewerWidget->setKpos(tgViewerWidget->getKpos());
	tgViewerWidget->setFocal(tgViewerWidget->getFocal());
	tgViewerWidget->setAperture(tgViewerWidget->getAperture());
	tgViewerWidget->setFov(tgViewerWidget->getFov());
	tgViewerWidget->setGamma(tgViewerWidget->getGamma());
	tgViewerWidget->setLWhite(tgViewerWidget->getLwhite());
	tgViewerWidget->setAlpha(tgViewerWidget->getAlpha());
	tgViewerWidget->setExposureMode(tgViewerWidget->getExposureMode());



}


void TGViewerWindow::renderTimeUpdated(int renderTime)
{
	QString label = QString("Rendering time %1 ms - %2 fps").arg(QString::number(renderTime), QString::number(1000/(renderTime+0.1)));
	renderTimeLabel->setText(label);
}

void TGViewerWindow::loadExrFile()
{
	QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open EXR File"), "",
        tr("Exr File (*.exr);;All Files (*)"));
	
	if (fileName.isEmpty())
        return;
	else
	{
		tgRenderer.readExrFile(fileName);
		tgViewerWidget->resize(tgRenderer.getWidth(), tgRenderer.getHeight());
	}
}