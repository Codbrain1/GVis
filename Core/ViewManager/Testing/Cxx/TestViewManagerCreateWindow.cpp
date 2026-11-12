#include "Core/ViewManager/ViewManager.h"
#include <QObject>
#include <QtTest>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qsignalspy.h>
#include <qtestcase.h>
#include <qtmetamacros.h>
#include "Enums/ViewEnums.h"
#include <memory>

class TestViewManagerCreateWindow : public QObject {
    Q_OBJECT
    std::unique_ptr<GVis::Core::ViewManager> m_viewManager;

  private slots:
    void init();
    void testCreateViewOpenGL3D();
};

void TestViewManagerCreateWindow::init() {
    m_viewManager = std::make_unique<GVis::Core::ViewManager>();
}

void TestViewManagerCreateWindow::testCreateViewOpenGL3D() {
    QVERIFY(m_viewManager);

    QSignalSpy spy(m_viewManager.get(), &GVis::Core::ViewManager::viewCreated);
    auto       viewId = m_viewManager->createView(GVis::Visualize::Views::ViewType::OpenGL3D);
    QVERIFY(!viewId.isNull());
    QCOMPARE(spy.count(), 1);

    auto view = m_viewManager->getView(viewId);
    QVERIFY(view);
    QCOMPARE(view->id(), viewId);
    QCOMPARE(view->viewType(), GVis::Visualize::Views::ViewType::OpenGL3D);

    auto signalViewId   = spy.at(0).at(0).value<QUuid>();
    auto signalViewType = spy.at(0).at(1).value<GVis::Visualize::Views::ViewType>();

    QCOMPARE(viewId, signalViewId);
    QCOMPARE(signalViewType, GVis::Visualize::Views::ViewType::OpenGL3D);

    QCOMPARE(view->viewType(), signalViewType);
}


QTEST_MAIN(TestViewManagerCreateWindow)
#include "TestViewManagerCreateWindow.moc"