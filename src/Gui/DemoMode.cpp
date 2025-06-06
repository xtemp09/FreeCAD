/***************************************************************************
 *   Copyright (c) 2010 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <limits>
#include <QCursor>
#include <QTimer>
#include <Inventor/nodes/SoCamera.h>
#endif

#include <Base/Tools.h>

#include "DemoMode.h"
#include "ui_DemoMode.h"
#include "Application.h"
#include "Command.h"
#include "Document.h"
#include "View3DInventor.h"
#include "View3DInventorViewer.h"


using namespace Gui::Dialog;

/* TRANSLATOR Gui::Dialog::DemoMode */

DemoMode::DemoMode(QWidget* /*parent*/, Qt::WindowFlags fl)
    : QDialog(nullptr, fl | Qt::WindowStaysOnTopHint)
    , viewAxis(0, 0, -1)
    , ui(new Ui_DemoMode)
{
    // create widgets
    ui->setupUi(this);
    setupConnections();
    ui->playButton->setCheckable(true);

    timer = new QTimer(this);
    timer->setInterval(1000 * ui->timeout->value());
    connect(timer, &QTimer::timeout, this, &DemoMode::onAutoPlay);
    oldvalue = ui->angleSlider->value();

    wasHidden = false;
    showHideTimer = new QTimer(this);
    showHideTimer->setInterval(5000);
    connect(showHideTimer, &QTimer::timeout, this, &DemoMode::hide);
}

/** Destroys the object and frees any allocated resources */
DemoMode::~DemoMode()
{
    delete ui;
}

void DemoMode::setupConnections()
{
    // clang-format off
    connect(ui->playButton, &QPushButton::clicked,
            this, &DemoMode::onPlayButtonToggled);
    connect(ui->fullscreen, &QCheckBox::toggled,
            this, &DemoMode::onFullscreenToggled);
    connect(ui->timerCheck, &QCheckBox::toggled,
            this, &DemoMode::onTimerCheckToggled);
    connect(ui->speedSlider, &QSlider::valueChanged,
            this, &DemoMode::onSpeedSliderValueChanged);
    connect(ui->angleSlider, &QSlider::valueChanged,
            this, &DemoMode::onAngleSliderValueChanged);
    connect(ui->timeout, qOverload<int>(&QSpinBox::valueChanged),
            this, &DemoMode::onTimeoutValueChanged);
    // clang-format on
}

void DemoMode::reset()
{
    onFullscreenToggled(false);
    Gui::View3DInventor* view = activeView();
    if (view) {
        view->getViewer()->stopAnimating();
    }
    ParameterGrp::handle hGrp =
        App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");
    hGrp->Notify("UseNavigationAnimations");
}

void DemoMode::accept()
{
    reset();
    QDialog::accept();
}

void DemoMode::reject()
{
    reset();
    QDialog::reject();
}

bool DemoMode::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseMove) {
        if (ui->fullscreen->isChecked()) {
            QPoint point = QCursor::pos() - oldPos;
            if (point.manhattanLength() > 5) {
                show();
                showHideTimer->start();
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void DemoMode::showEvent(QShowEvent*)
{
    if (this->wasHidden) {
        this->move(this->pnt);
    }
    this->wasHidden = false;
}

void DemoMode::hideEvent(QHideEvent*)
{
    this->pnt = this->pos();
    this->wasHidden = true;
    this->oldPos = QCursor::pos();
    showHideTimer->stop();
}

Gui::View3DInventor* DemoMode::activeView() const
{
    if (Document* doc = Application::Instance->activeDocument()) {
        return freecad_cast<Gui::View3DInventor*>(doc->getActiveView());
    }

    return nullptr;
}

float DemoMode::getSpeed(int v) const
{
    float speed = (static_cast<float>(v)) / 10.0f;  // let 10.0 be the maximum speed
    return speed;
}

SbVec3f DemoMode::getDirection(Gui::View3DInventor* view) const
{
    SoCamera* cam = view->getViewer()->getSoRenderManager()->getCamera();
    if (!cam) {
        return this->viewAxis;
    }
    SbRotation rot = cam->orientation.getValue();
    SbRotation inv = rot.inverse();
    SbVec3f vec(this->viewAxis);
    inv.multVec(vec, vec);
    if (vec.length() < std::numeric_limits<float>::epsilon()) {
        vec = this->viewAxis;
    }
    vec.normalize();
    return vec;
}

void DemoMode::onAngleSliderValueChanged(int v)
{
    Gui::View3DInventor* view = activeView();
    if (view) {
        SoCamera* cam = view->getViewer()->getSoRenderManager()->getCamera();
        if (!cam) {
            return;
        }
        auto angle = Base::toRadians<float>(/*90-v*/ v - this->oldvalue);
        SbRotation rot(SbVec3f(-1, 0, 0), angle);
        reorientCamera(cam, rot);
        this->oldvalue = v;
        if (view->getViewer()->isSpinning()) {
            startAnimation(view);
        }
    }
}

void DemoMode::reorientCamera(SoCamera* cam, const SbRotation& rot)
{
    // Find global coordinates of focal point.
    SbVec3f direction;
    cam->orientation.getValue().multVec(SbVec3f(0, 0, -1), direction);
    SbVec3f focalpoint = cam->position.getValue() + cam->focalDistance.getValue() * direction;

    // Set new orientation value by accumulating the new rotation.
    cam->orientation = rot * cam->orientation.getValue();

    // Reposition camera so we are still pointing at the same old focal point.
    cam->orientation.getValue().multVec(SbVec3f(0, 0, -1), direction);
    cam->position = focalpoint - cam->focalDistance.getValue() * direction;
}

void DemoMode::onSpeedSliderValueChanged(int v)
{
    Q_UNUSED(v);
    Gui::View3DInventor* view = activeView();
    if (view && view->getViewer()->isSpinning()) {
        startAnimation(view);
    }
}

void DemoMode::onPlayButtonToggled(bool pressed)
{
    Gui::View3DInventor* view = activeView();
    if (view) {
        if (pressed) {
            if (!view->getViewer()->isSpinning()) {
                SoCamera* cam = view->getViewer()->getSoRenderManager()->getCamera();
                if (cam) {
                    SbRotation rot = cam->orientation.getValue();
                    SbVec3f vec(0, -1, 0);
                    rot.multVec(vec, this->viewAxis);
                }
            }

            startAnimation(view);
            ui->playButton->setText(tr("Stop"));
        }
        else {
            view->getViewer()->stopAnimating();
            ui->playButton->setText(tr("Play"));
        }
    }
}

void DemoMode::onFullscreenToggled(bool on)
{
    Gui::View3DInventor* view = activeView();
    if (view) {
        CommandManager& rcCmdMgr = Application::Instance->commandManager();
        Command* cmd = rcCmdMgr.getCommandByName("Std_ViewDockUndockFullscreen");
        if (cmd) {
            cmd->invoke(on ? MDIView::FullScreen : MDIView::Child);
        }
        this->activateWindow();
        ui->playButton->setChecked(false);
    }
    if (on) {
        qApp->installEventFilter(this);
        showHideTimer->start();
    }
    else {
        qApp->removeEventFilter(this);
        showHideTimer->stop();
    }
}

void DemoMode::onTimeoutValueChanged(int v)
{
    timer->setInterval(v * 1000);
}

void DemoMode::onAutoPlay()
{
    Gui::View3DInventor* view = activeView();
    if (view && !view->getViewer()->isSpinning()) {
        ui->playButton->setChecked(true);
        startAnimation(view);
    }
}

void DemoMode::startAnimation(Gui::View3DInventor* view)
{
    view->getViewer()->startSpinningAnimation(getDirection(view),
                                              getSpeed(ui->speedSlider->value()));
}

void DemoMode::onTimerCheckToggled(bool on)
{
    if (on) {
        timer->start();
    }
    else {
        timer->stop();
    }
}

void DemoMode::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QDialog::changeEvent(e);
}

#include "moc_DemoMode.cpp"
