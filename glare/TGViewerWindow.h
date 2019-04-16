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
#include <QTimer>
#include "TGViewerWidget.h"

class TGViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    TGViewerWindow();

public slots:
	void renderTimeUpdated(int renderTime);
	void loadExrFile();

private:
	TGViewerWidget* tgViewerWidget;
    TemporalGlareRenderer tgRenderer;
	QLabel *cameraPosLabel;
	QDoubleSpinBox *apertureSB, *focalSB, *fovSB, *control1SB, *control2SB, *alphaSB;
    QLabel *renderTimeLabel;
	QLabel *focalLengthLabel;
};


#endif // WINDOW_H
