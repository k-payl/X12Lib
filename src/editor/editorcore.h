#ifndef EDITORGLOBAL_H
#define EDITORGLOBAL_H
#include "mainwindow.h"
#include "manipulators/imanipulator.h"

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QSet>

#include <chrono>

#include "core.h"
#include "resourcemanager.h"
#include "importthread.h"

class EditorCore;
class TreeNode;
class QApplication;

namespace engine {
	class Core;
}

extern EditorCore *editor;


enum class MANIPULATOR
{
	SELECT,
	TRANSLATE,
	ROTATE,
	SCALE
};


class EditorCore : public QObject
{
	Q_OBJECT

	QApplication& app_;
	bool isActive{true};
	bool preventFocusOnWorldLoad{false};

	QTimer *timer{nullptr};
	QTimer *timerEditorGUI{nullptr}; // gui
	std::chrono::steady_clock::time_point start;

	TreeNode *rootNode{nullptr};
	QHash<engine::GameObject*, TreeNode*> obj_to_treenode;

	QSet<engine::GameObject*> selectedObjects;

	MANIPULATOR maipulatorType_ = MANIPULATOR::SELECT;

	int progressBarLastValue{0};

	void onEngineInited();
	void onEngineFree();
	static void onEngineObejctAdded(engine::GameObject *obj);
	static void onEngineObejctDestroyed(engine::GameObject *obj);

public:
	EditorCore(QApplication& app);
	~EditorCore();

	engine::Core *core{nullptr};
	engine::ResourceManager *resMan{nullptr};

	MainWindow *window{nullptr};

	auto Init() -> void;
	auto Free() -> void;

	//----Engine-----

	auto LoadEngine() -> void;
	auto UnloadEngine() -> void;
	auto IsEngineLoaded() -> bool { return core != nullptr; }
	//auto ReloadCoreRender() -> void;
	auto ReloadShaders() -> void;
	auto Log(const char *message) -> void;
	auto RunImportFileTask(const QString& file)->void;

	// Global Progeress bar [0, 100]
	void SetProgerssBar(int progress);
	void SetProgressBarMessage(const QString& message);

	//----Objects-----

	// Nodes
	auto RootNode() -> TreeNode* { return rootNode; }
	auto TreNodeForObject(engine::GameObject *o) { return obj_to_treenode[o]; }

	// Game objects
	auto CreateGameObject() -> void;
	auto CreateModel(const char *path) -> void;
	auto CreateLight() -> void;
	auto CreateCamera() -> void;

	auto CloneSelectedGameObject() -> void;
	auto DestroyGameObject(engine::GameObject* obj) -> void;
	auto InsertRootGameObject(int row, engine::GameObject* obj) -> void;
	auto RemoveRootGameObject(engine::GameObject* obj) -> void;

	auto SaveWorld() -> void;
	auto CloseWorld() -> void;
	auto LoadWorld() -> void;

	// Selection objects
	auto SelectObjects(const QSet<engine::GameObject*>& objects) -> void;
	auto FirstSelectedObjects() -> engine::GameObject*;
	auto NumSelectedObjects() -> int { return selectedObjects.size(); }
	auto SelectionTransform() -> math::mat4;
	auto Focus() -> void;

	//---Manipulators---
	void ToggleManipulator(MANIPULATOR type);
	std::unique_ptr<IManupulator> currentManipulator;

signals:
	void OnUpdate(float dt);
	void OnRender();
	void OnEngineInstantiated(engine::Core* c);
	void OnEngineInit(engine::Core* c);
	void OnEngineFree(engine::Core* c);
	void OnObjectAdded(TreeNode *obj);
	void OnObjectRemoved(TreeNode *obj);
	void OnSelectionChanged(QSet<engine::GameObject*>& objects);
	void OnFocusOnSelected(const math::vec3& ceneter);

private slots:
	void OnTimer();
	void OnTimerEditorGUI();
	void OnAppStateChanged(Qt::ApplicationState state);
};

extern EditorCore *editor;


#endif // EDITORGLOBAL_H
