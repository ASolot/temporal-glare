#ifndef WINDOW_H
#define WINDOW_H

#include <QMainWindow>

#include "TemporalGlareRenderer.h"

#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QFileDialog>
#include <QComboBox>
#include "TGViewerWidget.h"

class TGViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    TGViewerWindow();

public slots:
	void KposChanged(QVector3D newKPos);
	void renderTimeUpdated(int renderTime);
	void fovUpdated(double fov);
	void loadExrFile();

private:
    TemporalGlareRenderer tgRenderer;
	QLabel *cameraPosLabel;
	QDoubleSpinBox *apertureSB, *focalSB, *fovSB, *control1SB, *control2SB;
    QLabel *renderTimeLabel;
	QLabel *focalLengthLabel;
};


#endif // WINDOW_H
