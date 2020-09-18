#ifndef SETTINGS_H
#define SETTINGS_H

#include <QWidget>

namespace engine{
class Core;
}
namespace Ui {
class Settings;
}

class Settings : public QWidget
{
	Q_OBJECT

public:
	explicit Settings(QWidget *parent = nullptr);
	~Settings();

	bool isWireframeAntialiasing() const;

private:
	Ui::Settings *ui;
private slots:
	void OnEngineFree(engine::Core *c);
	void OnEngineInit(engine::Core *c);
	void sliderOChanged(int i);
	void slider1Changed(int i);
};

#endif // SETTINGS_H
