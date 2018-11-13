#ifndef __MVME_ANALYSIS_UI_EVENTWIDGET_P_H__
#define __MVME_ANALYSIS_UI_EVENTWIDGET_P_H__

#include "analysis/analysis.h"
#include "analysis/analysis_ui_p.h"
#include "analysis/analysis_util.h"
#include "analysis/ui_eventwidget.h"
#include "analysis/ui_lib.h"
#include "mvme_stream_processor.h"

#include <QSplitter>
#include <QTreeWidget>
#include <QUuid>

namespace analysis
{
namespace ui
{

enum DataRole
{
    DataRole_AnalysisObject = Global_DataRole_AnalysisObject,
    DataRole_RawPointer,
    DataRole_ParameterIndex,
    DataRole_HistoAddress,
};

enum NodeType
{
    NodeType_Module = QTreeWidgetItem::UserType,
    NodeType_Source,
    NodeType_Operator,
    NodeType_OutputPipe,
    NodeType_OutputPipeParameter,

    NodeType_Histo1DSink,
    NodeType_Histo2DSink,
    NodeType_Sink,          // Sinks that are not handled specifically

    NodeType_Histo1D,

    NodeType_Directory,

    NodeType_MaxNodeType
};

class TreeNode: public CheckStateNotifyingNode
{
    public:
        using CheckStateNotifyingNode::CheckStateNotifyingNode;

        // Custom sorting for numeric columns
        virtual bool operator<(const QTreeWidgetItem &other) const override
        {
            if (type() == other.type() && treeWidget() && treeWidget()->sortColumn() == 0)
            {
                if (type() == NodeType_OutputPipeParameter)
                {
                    s32 thisAddress  = data(0, DataRole_ParameterIndex).toInt();
                    s32 otherAddress = other.data(0, DataRole_ParameterIndex).toInt();
                    return thisAddress < otherAddress;
                }
                else if (type() == NodeType_Histo1D)
                {
                    s32 thisAddress  = data(0, DataRole_HistoAddress).toInt();
                    s32 otherAddress = other.data(0, DataRole_HistoAddress).toInt();
                    return thisAddress < otherAddress;
                }
            }
            return CheckStateNotifyingNode::operator<(other);
        }
};

/* Specialized tree for the EventWidget.
 *
 * Note: the declaration is here because of MOC, the implementation is in analysis_ui.cc
 * because of locally defined types.
 */
class ObjectTree: public QTreeWidget, public CheckStateObserver
{
    Q_OBJECT
    public:
        using CheckStateChangeHandler = std::function<void (
            ObjectTree *tree, QTreeWidgetItem *node, const QVariant &prev)>;

        ObjectTree(QWidget *parent = nullptr)
            : QTreeWidget(parent)
        {}

        ObjectTree(EventWidget *eventWidget, s32 userLevel, QWidget *parent = nullptr)
            : QTreeWidget(parent)
            , m_eventWidget(eventWidget)
            , m_userLevel(userLevel)
        {}

        virtual ~ObjectTree() override;

        EventWidget *getEventWidget() const { return m_eventWidget; }
        void setEventWidget(EventWidget *widget) { m_eventWidget = widget; }
        MVMEContext *getContext() const;
        Analysis *getAnalysis() const;
        s32 getUserLevel() const { return m_userLevel; }
        void setUserLevel(s32 userLevel) { m_userLevel = userLevel; }
        QList<QTreeWidgetItem *> getTopLevelSelectedNodes() const;

        void setCheckStateChangeHandler(const CheckStateChangeHandler &csh) { m_csh = csh; }

        virtual void checkStateChanged(QTreeWidgetItem *node, const QVariant &prev) override
        {
            if (m_csh)
            {
                m_csh(this, node, prev);
            }
        }

    protected:
        virtual Qt::DropActions supportedDropActions() const override;
        virtual void dropEvent(QDropEvent *event) override;
        virtual void keyPressEvent(QKeyEvent *event) override;

    private:
        EventWidget *m_eventWidget = nullptr;
        s32 m_userLevel = 0;
        CheckStateChangeHandler m_csh;
};

class OperatorTree: public ObjectTree
{
    Q_OBJECT
    public:
        using ObjectTree::ObjectTree;

        virtual ~OperatorTree() override;

    protected:
        virtual QStringList mimeTypes() const override;

        virtual bool dropMimeData(QTreeWidgetItem *parent, int index,
                                  const QMimeData *data, Qt::DropAction action) override;

        virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
};

class DataSourceTree: public OperatorTree
{
    Q_OBJECT
    public:
        using OperatorTree::OperatorTree;

        virtual ~DataSourceTree() override;

        QTreeWidgetItem *unassignedDataSourcesRoot = nullptr;

    protected:
        virtual QStringList mimeTypes() const override;

        virtual bool dropMimeData(QTreeWidgetItem *parent, int index,
                                  const QMimeData *data, Qt::DropAction action) override;

        virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
};

class SinkTree: public ObjectTree
{
    Q_OBJECT
    public:
        using ObjectTree::ObjectTree;

        virtual ~SinkTree() override;

    protected:
        virtual QStringList mimeTypes() const override;

        virtual bool dropMimeData(QTreeWidgetItem *parent, int index,
                                  const QMimeData *data, Qt::DropAction action) override;

        virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
};

/* Operator (top) and Sink (bottom) trees showing objects for one userlevel. */
struct UserLevelTrees
{
    ObjectTree *operatorTree;
    SinkTree *sinkTree;
    s32 userLevel;

    std::array<ObjectTree *, 2> getObjectTrees() const
    {
        return
        {
            {
                reinterpret_cast<ObjectTree *>(operatorTree),
                reinterpret_cast<ObjectTree *>(sinkTree)
            }
        };
    }
};

using SetOfVoidStar = QSet<void *>;

struct EventWidgetPrivate
{
    enum Mode
    {
        /* Default mode interactions: add/remove objects, operations on selections,
         * drag and drop. */
        Default,

        /* An data extractor or operator add/edit dialog is active and waits
         * for input selection by the user. */
        SelectInput,
    };

    EventWidget *m_q;
    MVMEContext *m_context;
    QUuid m_eventId;
    int m_eventIndex;
    AnalysisWidget *m_analysisWidget;

    QVector<UserLevelTrees> m_levelTrees;
    ObjectToNode m_objectMap;

    Mode m_mode = Default;
    QWidget *m_uniqueWidget = nullptr;

    struct InputSelectInfo
    {
        Slot *slot = nullptr;
        s32 userLevel;
        EventWidget::SelectInputCallback callback;
        // Set of additional pipe sources to be considered invalid as valid
        // inputs for the slot.
        QSet<PipeSourceInterface *> additionalInvalidSources;
    };

    InputSelectInfo m_inputSelectInfo;

    ConditionLink m_applyConditionInfo;

    QSplitter *m_operatorFrameSplitter;
    QSplitter *m_displayFrameSplitter;

    enum TreeType
    {
        TreeType_Operator,
        TreeType_Sink,
        TreeType_Count
    };
    // Keeps track of the expansion state of those tree nodes that are storing objects in
    // DataRole_Pointer.
    // There's two sets, one for the operator trees and one for the display trees, because
    // objects may have nodes in both trees.
    // FIXME: does the case with two nodes really occur?
    std::array<SetOfVoidStar, TreeType_Count> m_expandedObjects;
    QTimer *m_displayRefreshTimer;

    // If set the trees for that user level will not be shown
    QVector<bool> m_hiddenUserLevels;

    // The user level that was manually added via addUserLevel()
    s32 m_manualUserLevel = 0;

    // Actions and widgets used in makeEventSelectAreaToolBar()
    QAction *m_actionExport;
    QAction *m_actionImport;
    QAction *m_actionSelectVisibleLevels;
    QLabel *m_eventRateLabel;

    QToolBar* m_upperToolBar;
    QToolBar* m_eventSelectAreaToolBar;

    // Periodically updated extractor hit counts and histo sink entry counts.
    struct ObjectCounters
    {
        QVector<double> hitCounts;
    };

    QHash<SourceInterface *, ObjectCounters> m_extractorCounters;
    QHash<Histo1DSink *, ObjectCounters> m_histo1DSinkCounters;
    QHash<Histo2DSink *, ObjectCounters> m_histo2DSinkCounters;
    MVMEStreamProcessorCounters m_prevStreamProcessorCounters;

    double m_prevAnalysisTimeticks = 0.0;;

    void createView(const QUuid &eventId);
    UserLevelTrees createTrees(const QUuid &eventId, s32 level);
    UserLevelTrees createSourceTrees(const QUuid &eventId);
    void appendTreesToView(UserLevelTrees trees);
    void repopulate();

    void addUserLevel();
    void removeUserLevel();
    s32 getUserLevelForTree(QTreeWidget *tree);

    void doOperatorTreeContextMenu(QTreeWidget *tree, QPoint pos, s32 userLevel);
    void doDataSourceOperatorTreeContextMenu(QTreeWidget *tree, QPoint pos, s32 userLevel);
    void doSinkTreeContextMenu(QTreeWidget *tree, QPoint pos, s32 userLevel);
    void doRawDataSinkTreeContextMenu(QTreeWidget *tree, QPoint pos, s32 userLevel);

    void setMode(Mode mode);
    Mode getMode() const;
    void modeChanged(Mode oldMode, Mode mode);
    void highlightValidInputNodes(QTreeWidgetItem *node);
    void highlightInputNodes(OperatorInterface *op);
    void highlightOutputNodes(PipeSourceInterface *ps);
    void clearToDefaultNodeHighlights(QTreeWidgetItem *node);
    void clearAllToDefaultNodeHighlights();
    void updateNodesForApplyConditionMode();
    void addConditionDecorations(const ConditionLink &cl);
    void removeConditionDecorations(const ConditionLink &cl);
    bool hasPendingConditionModifications() const;
    void onNodeClicked(TreeNode *node, int column, s32 userLevel);
    void onNodeDoubleClicked(TreeNode *node, int column, s32 userLevel);
    void onNodeChanged(TreeNode *node, int column, s32 userLevel);
    void onNodeCheckStateChanged(QTreeWidget *tree, QTreeWidgetItem *node, const QVariant &prev);
    void clearAllTreeSelections();
    void clearTreeSelectionsExcept(QTreeWidget *tree);
    void generateDefaultFilters(ModuleConfig *module);
    PipeDisplay *makeAndShowPipeDisplay(Pipe *pipe);
    void doPeriodicUpdate();
    void periodicUpdateExtractorCounters(double dt_s);
    void periodicUpdateHistoCounters(double dt_s);
    void periodicUpdateEventRate(double dt_s);
    void updateActions();

    // Object and node selections

    // Returns the currentItem() of the tree widget that has focus.
    QTreeWidgetItem *getCurrentNode() const;

    QList<QTreeWidgetItem *> getAllSelectedNodes() const;
    AnalysisObjectVector getAllSelectedObjects() const;

    QList<QTreeWidgetItem *> getTopLevelSelectedNodes() const;
    AnalysisObjectVector getTopLevelSelectedObjects() const;

    QVector<QTreeWidgetItem *> getCheckedNodes(
        Qt::CheckState checkState = Qt::Checked,
        int checkStateColumn = 0) const;

    AnalysisObjectVector getCheckedObjects(
        Qt::CheckState checkState = Qt::Checked,
        int checkStateColumn = 0) const;

    void clearSelections();
    void selectObjects(const AnalysisObjectVector &objects);

    // import / export
    bool canExport() const;
    void actionExport();
    void actionImport();

    void setSinksEnabled(const SinkVector &sinks, bool enabled);

    void removeSinks(const QVector<SinkInterface *> sinks);
    void removeDirectoryRecursively(const DirectoryPtr &dir);
    void removeObjects(const AnalysisObjectVector &objects);

    QTreeWidgetItem *findNode(const AnalysisObjectPtr &obj);

    void copyToClipboard(const AnalysisObjectVector &objects);
    bool canPaste();
    void pasteFromClipboard(QTreeWidget *destTree);

    Analysis *getAnalysis() const;
};

QString mode_to_string(EventWidgetPrivate::Mode mode);

} // ns ui
} // ns analysis

#endif /* __MVME_ANALYSIS_UI_EVENTWIDGET_P_H__ */