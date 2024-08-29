#pragma once

#include "Controls.h"

#include <QPushButton>
#include <QWidget>

class CCamera;

class ViewToolbar : public QWidget
{
  Q_OBJECT
public:
  ViewToolbar(QWidget* parent = nullptr);
  virtual ~ViewToolbar();
  void initFromCamera(const CCamera& camera);

  QPushButton* homeButton0;

  QPushButton* homeButton;
  QPushButton* frameViewButton;
  DualIconButton* orthoViewButton;
  QPushButton* topViewButton;
  QPushButton* bottomViewButton;
  QPushButton* frontViewButton;
  QPushButton* backViewButton;
  QPushButton* leftViewButton;
  QPushButton* rightViewButton;
  QPushButton* axisHelperButton;
};
