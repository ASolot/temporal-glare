#include "TGViewerWidget.h"
#include "TemporalGlareRenderer.h"
#include <algorithm>

#include <QPainter>
#include <QTimer>
#include <QTime>

#define LF_max_focal 15000.f
#define LF_min_focal 1.f

#define LF_min_fov 2.f
#define LF_max_fov 180.f


TGViewerWidget::TGViewerWidget(TemporalGlareRenderer *renderer, QWidget *parent)
    : QOpenGLWidget(parent), glRenderer(renderer)
{
    elapsed = 0;
		setMinimumSize(256, 256);
    setAutoFillBackground(false);
		setFocusPolicy(Qt::StrongFocus);
}

QSize TGViewerWidget::sizeHint() const
{
		return QSize(1024,1024);
}

void TGViewerWidget::animate()
{
    elapsed = (elapsed + qobject_cast<QTimer*>(sender())->interval()) % 1000;
}

void TGViewerWidget::setKpos(QVector3D newK_pos)
{
	glRenderer->K_pos = newK_pos;
	update();
	emit KposChanged(newK_pos);
}

void TGViewerWidget::setFocal(double newFocus)
{
	glRenderer->focus = newFocus;
	update();
	emit focalChanged(newFocus);
}


void TGViewerWidget::setAperture(double newAperture)
{
	glRenderer->apertureSize = newAperture;
	update();
	emit apertureChanged(newAperture);
}

void TGViewerWidget::setFov(double newFov)
{
	glRenderer->camera_fov = newFov;
	update();
	emit fovChanged(newFov);
}

void TGViewerWidget::paintEvent(QPaintEvent *event)
{
    QTime time;
    time.start();
    QPainter painter;
    painter.begin(this);
    painter.setRenderHint(QPainter::Antialiasing);
    glRenderer->paint(&painter, event, elapsed, size());
    painter.end();
		emit renderTimeUpdated( time.elapsed() );
}

void TGViewerWidget::keyPressEvent(QKeyEvent *event)
{
	QVector3D K_pos = getKpos();
	float focus = getFocal();

	const float posStep = .5f;
	const float focusStep = 10.f;
	const float apertureStep = 1.f;
	const float cameraFovStep = 1.f;
	if(event->key() == Qt::Key_A) {
		K_pos.setX(K_pos.x() - posStep);
		setKpos(K_pos);
    }
	if (event->key() == Qt::Key_D) {
		K_pos.setX(K_pos.x() + posStep);
		setKpos(K_pos);
	}
    if(event->key() == Qt::Key_W){
		K_pos.setY(K_pos.y() + posStep);
		setKpos(K_pos);
    }
    if(event->key() == Qt::Key_S){
		K_pos.setY(K_pos.y() - posStep);
		setKpos(K_pos);
	}
    if(event->key() == Qt::Key_Q){
		K_pos.setZ(K_pos.z() - posStep);
		setKpos(K_pos);
    }
    if(event->key() == Qt::Key_E){
		K_pos.setZ(K_pos.z() + posStep);
		setKpos(K_pos);
    }
    if(event->key() == Qt::Key_Left){
		setAperture(getAperture() - apertureStep);
    }
    if(event->key() == Qt::Key_Right){
		setAperture(getAperture() + apertureStep);	
    }
    if(event->key() == Qt::Key_Up){
		setFocal(std::min(getFocal() + focusStep, LF_max_focal));
    }
    if(event->key() == Qt::Key_Down){
		setFocal(std::max(getFocal() - focusStep, LF_min_focal));		
    }
	if (event->key() == Qt::Key_Minus) {
		setFov(std::max(getFov() - cameraFovStep, LF_min_fov));
	}
	if (event->key() == Qt::Key_Equal) {
		setFov(std::min(getFov() + cameraFovStep, LF_max_fov));
	}
	event->accept();
}

void TGViewerWidget::mousePressEvent(QMouseEvent * event)
{
	mouseDragStart = event->pos();
	mouseDragStartK_pos = glRenderer->K_pos;
	mouseDragFocal = glRenderer->focus;
	mouseDragButton = event->button();
	event->accept();
}

void TGViewerWidget::mouseMoveEvent(QMouseEvent * event)
{
	QPoint delta = event->pos() - mouseDragStart;

	if (mouseDragButton == Qt::RightButton) {
		float new_focal = mouseDragFocal - (float)delta.y() / 2.f;
		setFocal(std::min(std::max(new_focal, LF_min_focal), LF_max_focal));
	}
	else {
		QVector3D newK_pos;
		if (event->modifiers() == Qt::ShiftModifier) {
			newK_pos = mouseDragStartK_pos + QVector3D(delta.x(), delta.y(), 0.f) / 2.f;
		}
		else {
			newK_pos = mouseDragStartK_pos + QVector3D(delta.x(), 0.f, -delta.y()) / 2.f;
		}
		setKpos(newK_pos);
	}
	event->accept();
}

void TGViewerWidget::wheelEvent(QWheelEvent * event)
{
	QPoint numDegrees = event->angleDelta() / 8 / 15;

	double aperture = getAperture() + numDegrees.y();
	aperture = std::min(100., aperture);
	aperture = std::max(1., aperture);
	setAperture(aperture);	

	event->accept();	
}

QVector3D TGViewerWidget::getKpos()
{
	return glRenderer->K_pos;
}

float TGViewerWidget::getFocal()
{
	return glRenderer->focus;
}

float TGViewerWidget::getAperture()
{
	return glRenderer->apertureSize;
}

float TGViewerWidget::getFov()
{
	return glRenderer->camera_fov;
}