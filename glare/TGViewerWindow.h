#ifndef WINDOW_H
#define WINDOW_H

#include <QMainWindow>

#include "TemporalGlareRenderer.h"

#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include "TGViewerWidget.h"

class TGViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    TGViewerWindow();

	void loadTGData(QString TG_dir);

public slots:
	void KposChanged(QVector3D newKPos);
	void renderTimeUpdated(int renderTime);
	void fovUpdated(double fov);

private:
    TemporalGlareRenderer tgRenderer;
	QLabel *cameraPosLabel;
	QDoubleSpinBox *apertureSB, *focalSB, *fovSB;
    QLabel *renderTimeLabel;
	QLabel *focalLengthLabel;
};


#endif // WINDOW_H
