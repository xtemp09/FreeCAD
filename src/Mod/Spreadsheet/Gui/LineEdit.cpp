/***************************************************************************
 *   Copyright (c) 2015 Eivind Kvedalen <eivind@kvedalen.name>             *
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
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QGraphicsProxyWidget>
#include <QTimer>
#endif

#include "LineEdit.h"
#include <Gui/MainWindow.h>
#include "ZoomableView.h"

using namespace SpreadsheetGui;

LineEdit::LineEdit(QWidget* parent)
    : Gui::ExpressionLineEdit(parent, false, '=', true)
    , lastKeyPressed(0)
{
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
}

void LineEdit::setDocumentObject(const App::DocumentObject* currentDocObj, bool checkInList)
{
    ExpressionLineEdit::setDocumentObject(currentDocObj, checkInList);

    /* The code below is supposed to fix the input of an expression and to make the popup
     * functional. The input seems to be broken because of installed event filters. My solution is
     * to readd the widget into the scene. Only a parentless widget can be added to the scene.
     * Making a widget parentless makes it lose its windowFlags, even if it is added to the scene.
     * So, the algorithm is to obtain globalPos, then to make the widget parentless,
     * to add it to the scene, setting the windowFlags and the globalPos after.
     * */

    QPointer<Gui::MDIView> active_view = Gui::MainWindow::getInstance()->activeWindow();
    if (!active_view) {
        Base::Console().DeveloperWarning("LineEdit::setDocumentObject",
                                         "The active view is not Spreadsheet");
        return;
    }
    ZoomableView* zv = active_view->findChild<ZoomableView*>();
    if (zv == nullptr) {
        Base::Console().DeveloperWarning("LineEdit::setDocumentObject", "ZoomableView not found");
        return;
    }

    auto getPos = [this]() {
        return this->mapToGlobal(QPoint {0, 0});
    };
    const QPoint old_Pos = getPos();

    completer->setPopup(new XListView(this));
    setParent(nullptr);
    proxy_lineedit = zv->scene()->addWidget(this);
    setWindowFlag(Qt::FramelessWindowHint);

    const QPoint new_Pos = getPos();
    const QPoint shift = old_Pos - new_Pos;
    const qreal scale_coeff = 100.0 / static_cast<qreal>(zv->zoomLevel());
    const qreal shift_x = static_cast<qreal>(shift.x()) * scale_coeff,
                shift_y = static_cast<qreal>(shift.y()) * scale_coeff;

    QTimer::singleShot(20, this, [this, shift_x, shift_y]() {
        proxy_lineedit->moveBy(shift_x, shift_y);
        this->geometry_in_scene = proxy_lineedit->geometry();
    });

    auto updatePos = [this]() {
        proxy_lineedit->setGeometry(this->geometry_in_scene);
    };

    QObject::connect(zv, &ZoomableView::zoomLevelChanged, this, updatePos);
    QObject::connect(zv, &ZoomableView::scrollingOccured, this, updatePos);
    QObject::connect(zv, &ZoomableView::scrollingOccured, this, [this]() {
        lastKeyPressed = Qt::Key::Key_Enter;
        lastModifiers = Qt::KeyboardModifier::NoModifier;
        Q_EMIT finishedWithKey(lastKeyPressed, lastModifiers);
    });
}

bool LineEdit::eventFilter(QObject* object, QEvent* event)
{
    Q_UNUSED(object);
    if (event && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            // Special tab handling -- must be done via a QApplication event filter, otherwise the
            // widget system will always grab the tab events
            if (completerActive()) {
                hideCompleter();
                event->accept();
                return true;  // To make sure this tab press doesn't do anything else
            }
            else {
                lastKeyPressed = keyEvent->key();
                lastModifiers = keyEvent->modifiers();
            }
        }
    }
    return false;  // We don't usually actually "handle" the tab event, we just keep track of it
}

bool LineEdit::event(QEvent* event)
{
    if (event && event->type() == QEvent::FocusIn) {
        qApp->installEventFilter(this);
    }
    else if (event && event->type() == QEvent::FocusOut) {
        qApp->removeEventFilter(this);
        if (lastKeyPressed) {
            Q_EMIT finishedWithKey(lastKeyPressed, lastModifiers);
        }
        lastKeyPressed = 0;
    }
    else if (event && event->type() == QEvent::KeyPress && !completerActive()) {
        QKeyEvent* kevent = static_cast<QKeyEvent*>(event);
        lastKeyPressed = kevent->key();
        lastModifiers = kevent->modifiers();
    }
    return Gui::ExpressionLineEdit::event(event);
}

QPoint LineEdit::getPopupPos(void)
{
    QGraphicsView* zv = proxy_lineedit->scene()->views().first();
    const QPointF scenePos =
        proxy_lineedit->mapToScene(proxy_lineedit->boundingRect().bottomLeft());
    const QPoint viewPos = zv->mapFromScene(scenePos);
    const QPoint globalPos = zv->viewport()->mapToGlobal(viewPos);

    return globalPos;
}

XListView::XListView(LineEdit* parent)
    : QListView(parent)
{
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setAttribute(Qt::WidgetAttribute::WA_ShowWithoutActivating);

    QObject::connect(this, &XListView::geometryChanged, this, [this, parent]() {
        const QPoint new_pos = parent->getPopupPos();
        this->setGeometry(new_pos.x(), new_pos.y(), this->width(), this->height());
    });
}

void XListView::resizeEvent(QResizeEvent* event)
{
    Q_EMIT geometryChanged();
    QListView::resizeEvent(event);
}

void XListView::updateGeometries()
{
    QListView::updateGeometries();
    Q_EMIT geometryChanged();
}

#include "moc_LineEdit.cpp"
