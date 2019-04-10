#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <GL/glew.h>
#include <QOpenGLWidget>
#include <QKeyEvent>
#include "TemporalGlareRenderer.h"
#include <QLabel>
#include <QTimer>

class TGViewerWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    TGViewerWidget(TemporalGlareRenderer *glRenderer, QWidget *parent);
	QVector3D getKpos();
	float getFocal();
	float getAperture();
	float getFov();

public slots:
    void animate();
	void refresh();
	
	void setKpos(QVector3D newK_pos);  // Set a new position of the virtual camera
	void setFocal(double newFocus);  // Set a new focus distance for the virtual camera
	void setAperture(double newAperture);  // Set a new aperture for cameras
	void setFov(double newFov);  // Set a new aperture for cameras

signals:
	void KposChanged(QVector3D newK_pos);
signals:
	void focalChanged(double focus);
signals:
	void apertureChanged(double aperture);
signals:
	void fovChanged(double fov);
signals:
	void renderTimeUpdated(int renderTime);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event);

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	QSize sizeHint() const override;

private:
    TemporalGlareRenderer *glRenderer;
    int elapsed;
	QPoint mouseDragStart;
	Qt::MouseButton mouseDragButton;
	double mouseDragFocal;

	QTimer timer;
};

#endif // GLWIDGET_H
