#include "xdtsimportpopup.h"

#include "tapp.h"
#include "tsystem.h"
#include "toonzqt/filefield.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>

using namespace DVGui;

namespace {
QIcon getColorChipIcon(TPixel32 color) {
  QPixmap pm(15, 15);
  pm.fill(QColor(color.r, color.g, color.b));
  return QIcon(pm);
}
}  // namespace

//=============================================================================

XDTSImportPopup::XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                                 TFilePath scenePath, bool isUextVersion)
    : m_scene(scene)
    , m_isUext(isUextVersion)
    , DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "XDTSImport") {
  setWindowTitle(tr("Importing XDTS file %1")
                     .arg(QString::fromStdString(scenePath.getLevelName())));
  QPushButton* loadButton   = new QPushButton(tr("Load"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

  m_tick1Combo             = new QComboBox(this);
  m_tick2Combo             = new QComboBox(this);
  QList<QComboBox*> combos = {m_tick1Combo, m_tick2Combo};

  if (m_isUext) {
    m_keyFrameCombo       = new QComboBox(this);
    m_referenceFrameCombo = new QComboBox(this);
    combos << m_keyFrameCombo << m_referenceFrameCombo;
  }

  QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                ->getCurrentScene()
                                                ->getScene()
                                                ->getProperties()
                                                ->getCellMarks();
  for (auto combo : combos) {
    combo->addItem(tr("None"), -1);
    int curId = 0;
    for (auto mark : marks) {
      QString label = QString("%1: %2").arg(curId).arg(mark.name);
      combo->addItem(getColorChipIcon(mark.color), label, curId);
      curId++;
    }
  }
  m_tick1Combo->setCurrentIndex(m_tick1Combo->findData(6));
  m_tick2Combo->setCurrentIndex(m_tick2Combo->findData(8));
  if (m_isUext) {
    m_keyFrameCombo->setCurrentIndex(m_keyFrameCombo->findData(0));
    m_referenceFrameCombo->setCurrentIndex(m_referenceFrameCombo->findData(4));
  }

  QString description =
      tr("Please specify the level locations. Suggested paths "
         "are input in the fields with blue border.");

  m_topLayout->addWidget(new QLabel(description, this), 0);
  m_topLayout->addSpacing(15);

  QScrollArea* fieldsArea = new QScrollArea(this);
  fieldsArea->setWidgetResizable(true);

  QWidget* fieldsWidget = new QWidget(this);

  QGridLayout* fieldsLay = new QGridLayout();
  fieldsLay->setMargin(0);
  fieldsLay->setHorizontalSpacing(10);
  fieldsLay->setVerticalSpacing(10);
  fieldsLay->addWidget(new QLabel(tr("Level Name"), this), 0, 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
  fieldsLay->addWidget(new QLabel(tr("Level Path"), this), 0, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
  for (QString& levelName : levelNames) {
    int row = fieldsLay->rowCount();
    fieldsLay->addWidget(new QLabel(levelName, this), row, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
    FileField* fileField = new FileField(this);
    fieldsLay->addWidget(fileField, row, 1);
    m_fields.insert(levelName, fileField);
    fileField->setFileMode(QFileDialog::AnyFile);
    fileField->setObjectName("SuggestiveFileField");
    connect(fileField, SIGNAL(pathChanged()), this, SLOT(onPathChanged()));
  }
  fieldsLay->setColumnStretch(1, 1);
  fieldsLay->setRowStretch(fieldsLay->rowCount(), 1);

  fieldsWidget->setLayout(fieldsLay);
  fieldsArea->setWidget(fieldsWidget);
  m_topLayout->addWidget(fieldsArea, 1);

  // ���T ����^�����Q�l�̃R�}�}�[�N�̃��C�A�E�g����I

  // cell mark area
  QGroupBox* cellMarkGroupBox =
      new QGroupBox(tr("Cell marks for XDTS symbols"));
  QGridLayout* markLay = new QGridLayout();
  markLay->setMargin(10);
  markLay->setVerticalSpacing(10);
  markLay->setHorizontalSpacing(5);
  {
    markLay->addWidget(new QLabel(tr("Inbetween Symbol1 (O):"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    markLay->addWidget(m_tick1Combo, 0, 1);
    markLay->addItem(new QSpacerItem(10, 1), 0, 2);
    markLay->addWidget(new QLabel(tr("Inbetween Symbol2 (*)"), this), 0, 3,
                       Qt::AlignRight | Qt::AlignVCenter);
    markLay->addWidget(m_tick2Combo, 0, 4);

    if (m_isUext) {
      markLay->addWidget(new QLabel(QObject::tr("Keyframe Symbol:")), 1, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
      markLay->addWidget(m_keyFrameCombo, 1, 1);
      markLay->addWidget(new QLabel(QObject::tr("Reference Frame Symbol:")), 1,
                         3, Qt::AlignRight | Qt::AlignVCenter);
      markLay->addWidget(m_referenceFrameCombo, 1, 4);
    }
  }
  cellMarkGroupBox->setLayout(markLay);

  m_topLayout->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

  connect(loadButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(loadButton, cancelButton);

  updateSuggestions(scenePath.getQString());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::onPathChanged() {
  FileField* fileField = dynamic_cast<FileField*>(sender());
  if (!fileField) return;
  QString levelName = m_fields.key(fileField);
  // make the field non-suggestive
  m_pathSuggestedLevels.removeAll(levelName);

  // if the path is specified under the sub-folder with the same name as the
  // level, then try to make suggestions from the parent folder of it
  TFilePath fp =
      m_scene->decodeFilePath(TFilePath(fileField->getPath())).getParentDir();
  if (QDir(fp.getQString()).dirName() == levelName)
    updateSuggestions(fp.getQString());

  updateSuggestions(fileField->getPath());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::updateSuggestions(const QString samplePath) {
  TFilePath fp(samplePath);
  fp = m_scene->decodeFilePath(fp).getParentDir();
  QDir suggestFolder(fp.getQString());
  QStringList filters;
  filters << "*.bmp"
          << "*.jpg"
          << "*.nol"
          << "*.pic"
          << "*.pict"
          << "*.pct"
          << "*.png"
          << "*.rgb"
          << "*.sgi"
          << "*.tga"
          << "*.tif"
          << "*.tiff"
          << "*.tlv"
          << "*.pli"
          << "*.psd";
  suggestFolder.setNameFilters(filters);
  suggestFolder.setFilter(QDir::Files);
  TFilePathSet pathSet;
  try {
    TSystem::readDirectory(pathSet, suggestFolder, true);
  } catch (...) {
    return;
  }

  QMap<QString, FileField*>::iterator fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    QString levelName    = fieldsItr.key();
    FileField* fileField = fieldsItr.value();
    // check if the field can be filled with suggestion
    if (fileField->getPath().isEmpty() ||
        m_pathSuggestedLevels.contains(levelName)) {
      // input suggestion if there is a file with the same level name
      bool found = false;
      for (TFilePath path : pathSet) {
        if (path.getName() == levelName.toStdString()) {
          TFilePath codedPath = m_scene->codeFilePath(path);
          fileField->setPath(codedPath.getQString());
          if (!m_pathSuggestedLevels.contains(levelName))
            m_pathSuggestedLevels.append(levelName);
          found = true;
          break;
        }
      }
      // Not found in the current folder.
      // Then check if there is a sub-folder with the same name as the level
      // (like foo/A/A.tlv), as CSP exports levels like that.
      if (!found && suggestFolder.cd(levelName)) {
        TFilePathSet subPathSet;
        try {
          TSystem::readDirectory(subPathSet, suggestFolder, true);
        } catch (...) {
          return;
        }
        for (TFilePath path : subPathSet) {
          if (path.getName() == levelName.toStdString()) {
            TFilePath codedPath = m_scene->codeFilePath(path);
            fileField->setPath(codedPath.getQString());
            if (!m_pathSuggestedLevels.contains(levelName))
              m_pathSuggestedLevels.append(levelName);
            break;
          }
        }
        // back to parent folder
        suggestFolder.cdUp();
      }
    }
    ++fieldsItr;
  }

  // repaint fields
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    if (m_pathSuggestedLevels.contains(fieldsItr.key()))
      fieldsItr.value()->setStyleSheet(
          QString("#SuggestiveFileField "
                  "QLineEdit{border-color:#2255aa;border-width:2px;}"));
    else
      fieldsItr.value()->setStyleSheet(QString(""));
    ++fieldsItr;
  }
}

//-----------------------------------------------------------------------------

QString XDTSImportPopup::getLevelPath(QString levelName) {
  FileField* field = m_fields.value(levelName);
  if (!field) return QString();
  return field->getPath();
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::getMarkerIds(int& tick1Id, int& tick2Id, int& keyFrameId,
                                   int& referenceFrameId) {
  tick1Id = m_tick1Combo->currentData().toInt();
  tick2Id = m_tick2Combo->currentData().toInt();
  if (m_isUext) {
    keyFrameId       = m_keyFrameCombo->currentData().toInt();
    referenceFrameId = m_referenceFrameCombo->currentData().toInt();
  } else {
    keyFrameId       = -1;
    referenceFrameId = -1;
  }
}