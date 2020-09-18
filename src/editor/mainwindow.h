#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>

class Settings;

namespace engine{
	class Core;
}

enum THEME
{
	LIGHT = 0,
	DARK = 1
};

enum EDITOR_WINDOWS
{
	VIEWPORT_0 = 0,
//	VIEWPORT_1,
//	VIEWPORT_2,
//	VIEWPORT_3,
	VIEWPORT_NUM,
	CONSOLE = VIEWPORT_NUM,
	SCENETREE,
	PARAMETERS,
	DEBUG_WIDGET,
	PROJECT_WIDGET,
	SETTINGS,
	WINDOWS_NUM
};

void load_theme(QString name);


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

	QProgressBar *progressBar;
	int progress_{};

	THEME theme{};

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	WId getWindowHandle();
	Settings *GetSettings();

	// [0, 100]
	void SetProgressBar(int progress);
	void HideProgressBar();
	void ShowProgressBar();

protected:
	virtual void closeEvent(QCloseEvent* e);

private slots:

	void on_actionClose_Scene_triggered();

	void on_actionSave_World_triggered();

	void on_actionLoad_World_triggered();

	void on_actionCreate_GameObject_triggered();

	void on_actionCreate_Light_triggered();

	void on_actionCreate_Camera_triggered();

	void on_actionCreate_Model_triggered();


	void on_actionselect_triggered(bool checked);

	void on_actionTranslator_triggered(bool checked);

	void on_actionReload_shaders_triggered();

	void on_actionactionRotator_triggered(bool checked);

	void on_actionscale_triggered();

	void on_actionscale_triggered(bool checked);

private:
	Ui::MainWindow *ui;

	void applyDarkStyle();
	void applyLightStyle();
	void switch_button(QAction *action);

public slots:
//	void keyPressEvent(QKeyEvent*);
//	void keyReleaseEvent(QKeyEvent*);
	bool MainWindow::eventFilter(QObject *watched, QEvent *event) override;
	void OnEngineInit(engine::Core* c);

signals:
	void onKeyPressed(Qt::Key key);
	void onKeyReleased(Qt::Key key);

};

#endif // MAINWINDOW_H
