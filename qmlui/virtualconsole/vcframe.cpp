/*
  Q Light Controller Plus
  vcframe.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QQmlEngine>
#include <QDebug>
#include <QtXml>

#include "vcframe.h"
#include "vclabel.h"
#include "vcbutton.h"
#include "vcsoloframe.h"
#include "virtualconsole.h"

VCFrame::VCFrame(Doc *doc, VirtualConsole *vc, QObject *parent)
    : VCWidget(doc, parent)
    , m_vc(vc)
    , m_hasSoloParent(false)
    , m_showHeader(true)
    , m_showEnable(true)
    , m_isCollapsed(false)
    , m_multiPageMode(false)
    , m_currentPage(0)
    , m_totalPagesNumber(1)
    , m_pagesLoop(false)
{
    setType(VCWidget::FrameWidget);
}

VCFrame::~VCFrame()
{
}

void VCFrame::render(QQuickView *view, QQuickItem *parent)
{
    if (view == NULL || parent == NULL)
        return;

    QQmlComponent *component = new QQmlComponent(view->engine(), QUrl("qrc:/VCFrameItem.qml"));

    if (component->isError())
    {
        qDebug() << component->errors();
        return;
    }

    QQuickItem *item = qobject_cast<QQuickItem*>(component->create());

    item->setParentItem(parent);
    item->setProperty("frameObj", QVariant::fromValue(this));

    if (m_pagesMap.count() > 0)
    {
        QString chName = QString("frameDropArea%1").arg(id());
        QQuickItem *childrenArea = qobject_cast<QQuickItem*>(item->findChild<QObject *>(chName));

        foreach(VCWidget *child, m_pagesMap.keys())
            child->render(view, childrenArea);
    }
}

void VCFrame::setHasSoloParent(bool hasSoloParent)
{
    m_hasSoloParent = hasSoloParent;
}

bool VCFrame::hasSoloParent() const
{
    return m_hasSoloParent;
}

bool VCFrame::hasChildren()
{
    return !m_pagesMap.isEmpty();
}

QList<VCWidget *> VCFrame::children(bool recursive)
{
    QList<VCWidget *> widgetsList;

    if (recursive == false)
        return m_pagesMap.keys();
    else
    {
        foreach(VCWidget *widget, m_pagesMap.keys())
        {
            widgetsList.append(widget);
            if (widget->type() == FrameWidget || widget->type() == SoloFrameWidget)
            {
                VCFrame *frame = qobject_cast<VCFrame *>(widget);
                widgetsList.append(frame->children(true));
            }
        }
    }

    return widgetsList;
}

void VCFrame::addWidget(QQuickItem *parent, QString wType, QPoint pos)
{
    qDebug() << "[VCFrame] adding widget of type:" << wType << pos;
    m_vc->resetDropTargets(true);

    VCWidget::WidgetType type = stringToType(wType);

    switch (type)
    {
        case FrameWidget:
        {
            VCFrame *frame = new VCFrame(m_doc, m_vc, this);
            QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);
            frame->setGeometry(QRect(pos.x(), pos.y(), 300, 300));
            setupWidget(frame);
            m_vc->addWidgetToMap(frame);
            frame->render(m_vc->view(), parent);
        }
        break;
        case SoloFrameWidget:
        {
            VCSoloFrame *soloframe = new VCSoloFrame(m_doc, m_vc, this);
            QQmlEngine::setObjectOwnership(soloframe, QQmlEngine::CppOwnership);
            soloframe->setGeometry(QRect(pos.x(), pos.y(), 300, 300));
            setupWidget(soloframe);
            m_vc->addWidgetToMap(soloframe);
            soloframe->render(m_vc->view(), parent);
        }
        break;
        case ButtonWidget:
        {
            VCButton *button = new VCButton(m_doc, this);
            QQmlEngine::setObjectOwnership(button, QQmlEngine::CppOwnership);
            button->setGeometry(QRect(pos.x(), pos.y(), 100, 100));
            setupWidget(button);
            m_vc->addWidgetToMap(button);
            button->render(m_vc->view(), parent);
        }
        break;
        case LabelWidget:
        {
            VCLabel *label = new VCLabel(m_doc, this);
            QQmlEngine::setObjectOwnership(label, QQmlEngine::CppOwnership);
            label->setGeometry(QRect(pos.x(), pos.y(), 100, 100));
            setupWidget(label);
            m_vc->addWidgetToMap(label);
            label->render(m_vc->view(), parent);
        }
        break;
        default:
        break;
    }
}

void VCFrame::deleteChildren()
{
    if (m_pagesMap.isEmpty())
        return;

    QMapIterator <VCWidget*, int> it(m_pagesMap);
    while (it.hasNext() == true)
    {
        it.next();
        VCWidget *widget = it.key();
        if(widget->type() == FrameWidget)
        {
            VCFrame *frame = static_cast<VCFrame*>(widget);
            frame->deleteChildren();
        }
        else if(widget->type() == SoloFrameWidget)
        {
            VCFrame *soloframe = static_cast<VCFrame*>(widget);
            soloframe->deleteChildren();
        }
        m_pagesMap.remove(widget);
        delete widget;
    }
}

void VCFrame::setupWidget(VCWidget *widget)
{
    m_pagesMap.insert(widget, widget->page());

    // if we're a normal Frame and we have a Solo Frame parent
    // then passthrough the widget functionStarting signal.
    // If we're not into a Solo Frame parent, then don't even connect
    // the signal, so each widget will know to immediately start the Function
    if (xmlTagName() == KXMLQLCVCFrame && m_hasSoloParent == true)
    {
        connect(widget, SIGNAL(functionStarting(VCWidget *,quint32,qreal)),
                this, SIGNAL(functionStarting(VCWidget *,quint32,qreal)));
    }

    // otherwise, if we're a Solo Frame, connect the widget
    // functionStarting signal to a slot to handle the event
    if (xmlTagName() == KXMLQLCVCSoloFrame)
    {
        connect(widget, SIGNAL(functionStarting(VCWidget *,quint32,qreal)),
                this, SLOT(slotFunctionStarting(VCWidget *,quint32,qreal)));
    }

}

void VCFrame::deleteWidget(VCWidget *widget)
{
    m_pagesMap.remove(widget);
}

/*********************************************************************
 * Header
 *********************************************************************/

bool VCFrame::showHeader() const
{
    return m_showHeader;
}

void VCFrame::setShowHeader(bool showHeader)
{
    if (m_showHeader == showHeader)
        return;

    m_showHeader = showHeader;
    emit showHeaderChanged(showHeader);
}

/*********************************************************************
 * Enable button
 *********************************************************************/

bool VCFrame::showEnable() const
{
    return m_showEnable;
}

void VCFrame::setShowEnable(bool showEnable)
{
    if (m_showEnable == showEnable)
        return;

    m_showEnable = showEnable;
    emit showEnableChanged(showEnable);
}

/*********************************************************************
 * Collapsed state
 *********************************************************************/

bool VCFrame::isCollapsed() const
{
    return m_isCollapsed;
}

void VCFrame::setCollapsed(bool isCollapsed)
{
    if (m_isCollapsed == isCollapsed)
        return;

    m_isCollapsed = isCollapsed;
    emit collapsedChanged(isCollapsed);
    setDocModified();
}

/*********************************************************************
 * Multi page mode
 *********************************************************************/

bool VCFrame::multiPageMode() const
{
    return m_multiPageMode;
}

void VCFrame::setMultiPageMode(bool multiPageMode)
{
    if (m_multiPageMode == multiPageMode)
        return;

    m_multiPageMode = multiPageMode;
    emit multiPageModeChanged(multiPageMode);
}

void VCFrame::setTotalPagesNumber(int num)
{
    m_totalPagesNumber = num;
}

int VCFrame::totalPagesNumber() const
{
    return m_totalPagesNumber;
}

int VCFrame::currentPage() const
{
    if (m_multiPageMode == false)
        return 0;
    return m_currentPage;
}

void VCFrame::setCurrentPage(int pageNum)
{
    if (pageNum < 0 || pageNum >= m_totalPagesNumber)
        return;

    m_currentPage = pageNum;

    QMapIterator <VCWidget*, int> it(m_pagesMap);
    while (it.hasNext() == true)
    {
        it.next();
        int page = it.value();
        VCWidget *widget = it.key();
        if (page == m_currentPage)
        {
            widget->setDisabled(false);
            widget->setVisible(true);
            //widget->updateFeedback();
        }
        else
        {
            widget->setDisabled(true);
            widget->setVisible(false);
        }
    }
    setDocModified();
    emit currentPageChanged(m_currentPage);
}

void VCFrame::setPagesLoop(bool pagesLoop)
{
    m_pagesLoop = pagesLoop;
}

bool VCFrame::pagesLoop() const
{
    return m_pagesLoop;
}

void VCFrame::gotoPreviousPage()
{
    if (m_pagesLoop && m_currentPage == 0)
        setCurrentPage(m_totalPagesNumber - 1);
    else
        setCurrentPage(m_currentPage - 1);

    //sendFeedback(m_currentPage, previousPageInputSourceId);
}

void VCFrame::gotoNextPage()
{
    if (m_pagesLoop && m_currentPage == m_totalPagesNumber - 1)
        setCurrentPage(0);
    else
        setCurrentPage(m_currentPage + 1);

    //sendFeedback(m_currentPage, nextPageInputSourceId);
}

/*********************************************************************
 * Widget Function
 *********************************************************************/

void VCFrame::slotFunctionStarting(VCWidget *widget, quint32 fid, qreal fIntensity)
{
    Q_UNUSED(widget)
    Q_UNUSED(fid)
    Q_UNUSED(fIntensity)

    if (xmlTagName() == KXMLQLCVCFrame)
        qDebug() << "[VCFrame] ERROR ! This should never happen !";
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

QString VCFrame::xmlTagName() const
{
    return KXMLQLCVCFrame;
}

bool VCFrame::loadXML(const QDomElement* root)
{
    Q_ASSERT(root != NULL);

    if (root->tagName() != xmlTagName())
    {
        qWarning() << Q_FUNC_INFO << "Frame node not found";
        return false;
    }

    /* Widget commons */
    loadXMLCommon(root);

    /* Children */
    QDomNode node = root->firstChild();
    int currentPage = 0;

    while (node.isNull() == false)
    {
        QDomElement tag = node.toElement();
        if (tag.tagName() == KXMLQLCWindowState)
        {
            /* Frame geometry (visibility is ignored) */
            int x = 0, y = 0, w = 0, h = 0;
            bool visible = false;
            loadXMLWindowState(&tag, &x, &y, &w, &h, &visible);
            setGeometry(QRect(x, y, w, h));
        }
        else if (tag.tagName() == KXMLQLCVCWidgetAppearance)
        {
            /* Frame appearance */
            loadXMLAppearance(&tag);
        }
        else if (tag.tagName() == KXMLQLCVCFrameShowHeader)
        {
            if (tag.text() == KXMLQLCTrue)
                setShowHeader(true);
            else
                setShowHeader(false);
        }
        else if (tag.tagName() == KXMLQLCVCFrameIsCollapsed)
        {
            /* Collapsed */
            if (tag.text() == KXMLQLCTrue)
                setCollapsed(true);
        }
        else if (tag.tagName() == KXMLQLCVCFrameShowEnableButton)
        {
            if (tag.text() == KXMLQLCTrue)
                setShowEnable(true);
            else
                setShowEnable(false);
        }
        else if (tag.tagName() == KXMLQLCVCFrameMultipage)
        {
            setMultiPageMode(true);
            if (tag.hasAttribute(KXMLQLCVCFramePagesNumber))
                setTotalPagesNumber(tag.attribute(KXMLQLCVCFramePagesNumber).toInt());

            if(tag.hasAttribute(KXMLQLCVCFrameCurrentPage))
                currentPage = tag.attribute(KXMLQLCVCFrameCurrentPage).toInt();
        }

        /** ***************** children widgets *************************** */

        else if (tag.tagName() == KXMLQLCVCFrame)
        {
            /* Create a new frame into its parent */
            VCFrame* frame = new VCFrame(m_doc, m_vc, this);

            // if we're a Solo Frame or we have a Solo Frame parent, set
            // the new frame accordingly
            if (xmlTagName() == KXMLQLCVCSoloFrame || m_hasSoloParent == true)
                frame->setHasSoloParent(true);

            if (frame->loadXML(&tag) == false)
                delete frame;
            else
            {
                QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);
                setupWidget(frame);
                m_vc->addWidgetToMap(frame);
            }
        }
        else if (tag.tagName() == KXMLQLCVCSoloFrame)
        {
            /* Create a new frame into its parent */
            VCSoloFrame* soloframe = new VCSoloFrame(m_doc, m_vc, this);
            if (soloframe->loadXML(&tag) == false)
                delete soloframe;
            else
            {
                QQmlEngine::setObjectOwnership(soloframe, QQmlEngine::CppOwnership);
                setupWidget(soloframe);
                m_vc->addWidgetToMap(soloframe);
            }
        }
        else if (tag.tagName() == KXMLQLCVCButton)
        {
            /* Create a new button into its parent */
            VCButton* button = new VCButton(m_doc, this);
            if (button->loadXML(&tag) == false)
                delete button;
            else
            {
                QQmlEngine::setObjectOwnership(button, QQmlEngine::CppOwnership);
                setupWidget(button);
                m_vc->addWidgetToMap(button);
            }
        }
        else if (tag.tagName() == KXMLQLCVCLabel)
        {
            /* Create a new label into its parent */
            VCLabel* label = new VCLabel(m_doc, this);
            if (label->loadXML(&tag) == false)
                delete label;
            else
            {
                QQmlEngine::setObjectOwnership(label, QQmlEngine::CppOwnership);
                setupWidget(label);
                m_vc->addWidgetToMap(label);
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown frame tag:" << tag.tagName();
        }

        node = node.nextSibling();
    }

    if (multiPageMode() == true)
        setCurrentPage(currentPage);

    return true;
}


