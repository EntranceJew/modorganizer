/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "settings.h"

#include "settingsdialog.h"
#include "utility.h"
#include "helper.h"
#include <gameinfo.h>
#include <appconfig.h>
#include <utility.h>

#include <QCheckBox>
#include <QLineEdit>
#include <QDirIterator>
#include <QRegExp>
#include <QCoreApplication>
#include <QMessageBox>


using namespace MOBase;
using namespace MOShared;


static const unsigned char Key2[20] = { 0x99, 0xb8, 0x76, 0x42, 0x3e, 0xc1, 0x60, 0xa4, 0x5b, 0x01,
                                        0xdb, 0xf8, 0x43, 0x3a, 0xb7, 0xb6, 0x98, 0xd4, 0x7d, 0xa2 };

Settings *Settings::s_Instance = NULL;


Settings::Settings()
  : m_Settings(ToQString(GameInfo::instance().getIniFilename()), QSettings::IniFormat)
{
  if (s_Instance != NULL) {
    throw std::runtime_error("second instance of \"Settings\" created");
  } else {
    s_Instance = this;
  }
}


Settings::~Settings()
{
  s_Instance = NULL;
}


Settings &Settings::instance()
{
  if (s_Instance == NULL) {
    throw std::runtime_error("no instance of \"Settings\"");
  }
  return *s_Instance;
}

void Settings::clearPlugins()
{
  m_Plugins.clear();
  m_PluginSettings.clear();
}

void Settings::registerPlugin(IPlugin *plugin)
{
  m_Plugins.push_back(plugin);
  foreach (const PluginSetting &setting, plugin->settings()) {
    QVariant temp = m_Settings.value("Plugins/" + plugin->name() + "/" + setting.key, setting.defaultValue);
    if (!temp.convert(setting.defaultValue.type())) {
      qWarning("failed to interpret \"%s\" as correct type for \"%s\" in plugin \"%s\", using default",
               qPrintable(temp.toString()), qPrintable(setting.key), qPrintable(plugin->name()));
      temp = setting.defaultValue;
    }
    m_PluginSettings[plugin->name()][setting.key] = temp;
  }
}


QString Settings::obfuscate(const QString &password) const
{
  QByteArray temp = password.toUtf8();

  QByteArray buffer;
  for (int i = 0; i < temp.length(); ++i) {
    buffer.append(temp.at(i) ^ Key2[i % 20]);
  }
  return buffer.toBase64();
}


QString Settings::deObfuscate(const QString &password) const
{
  QByteArray temp(QByteArray::fromBase64(password.toUtf8()));

  QByteArray buffer;
  for (int i = 0; i < temp.length(); ++i) {
    buffer.append(temp.at(i) ^ Key2[i % 20]);
  }
  return QString::fromUtf8(buffer.constData());
}


bool Settings::hideUncheckedPlugins() const
{
  return m_Settings.value("Settings/hide_unchecked_plugins", false).toBool();
}

bool Settings::forceEnableCoreFiles() const
{
  return m_Settings.value("Settings/force_enable_core_files", true).toBool();
}

bool Settings::automaticLoginEnabled() const
{
  return m_Settings.value("Settings/nexus_login", false).toBool();
}

QString Settings::getSteamAppID() const
{
  return m_Settings.value("Settings/app_id", ToQString(GameInfo::instance().getSteamAPPId())).toString();
}

QString Settings::getDownloadDirectory() const
{
  return QDir::toNativeSeparators(m_Settings.value("Settings/download_directory", ToQString(GameInfo::instance().getDownloadDir())).toString());
}

QString Settings::getCacheDirectory() const
{
  return QDir::toNativeSeparators(m_Settings.value("Settings/cache_directory", ToQString(GameInfo::instance().getCacheDir())).toString());
}

QString Settings::getModDirectory() const
{
  return QDir::toNativeSeparators(m_Settings.value("Settings/mod_directory", ToQString(GameInfo::instance().getModsDir())).toString());
}

QString Settings::getNMMVersion() const
{
  return m_Settings.value("Settings/nmm_version", "0.34.0").toString();
}

bool Settings::getNexusLogin(QString &username, QString &password) const
{
  if (m_Settings.value("Settings/nexus_login", false).toBool()) {
    username = m_Settings.value("Settings/nexus_username", "").toString();
    password = deObfuscate(m_Settings.value("Settings/nexus_password", "").toString());
    return true;
  } else {
    return false;
  }
}

int Settings::logLevel() const
{
  return m_Settings.value("Settings/log_level", 0).toInt();
}


void Settings::setNexusLogin(QString username, QString password)
{
  m_Settings.setValue("Settings/nexus_login", true);
  m_Settings.setValue("Settings/nexus_username", username);
  m_Settings.setValue("Settings/nexus_password", obfuscate(password));
}


LoadMechanism::EMechanism Settings::getLoadMechanism() const
{
  switch (m_Settings.value("Settings/load_mechanism").toInt()) {
    case LoadMechanism::LOAD_MODORGANIZER: return LoadMechanism::LOAD_MODORGANIZER;
    case LoadMechanism::LOAD_SCRIPTEXTENDER: return LoadMechanism::LOAD_SCRIPTEXTENDER;
    case LoadMechanism::LOAD_PROXYDLL: return LoadMechanism::LOAD_PROXYDLL;
  }
  throw std::runtime_error("invalid load mechanism");
}


void Settings::setupLoadMechanism()
{
  m_LoadMechanism.activate(getLoadMechanism());
}


bool Settings::preferIntegratedInstallers()
{
  return m_Settings.value("Settings/prefer_integrated_installer").toBool();
}


bool Settings::enableQuickInstaller()
{
  return m_Settings.value("Settings/enable_quick_installer").toBool();
}


bool Settings::preferExternalBrowser()
{
  return m_Settings.value("Settings/prefer_external_browser").toBool();
}


void Settings::setMotDHash(uint hash)
{
  m_Settings.setValue("motd_hash", hash);
}

uint Settings::getMotDHash() const
{
  return m_Settings.value("motd_hash", 0).toUInt();
}

QVariant Settings::pluginSetting(const QString &pluginName, const QString &key) const
{
  auto iterPlugin = m_PluginSettings.find(pluginName);
  if (iterPlugin == m_PluginSettings.end()) {
    throw MyException(tr("setting for invalid plugin \"%1\" requested").arg(key));
  }
  auto iterSetting = iterPlugin->find(key);
  if (iterSetting == iterPlugin->end()) {
    throw MyException(tr("invalid setting \"%1\" requested for plugin \"%2\"").arg(key).arg(pluginName));
  }

  return *iterSetting;
}


void Settings::addLanguages(QComboBox *languageBox)
{
  languageBox->addItem("English", "en_US");

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/translations", QDir::Files);
  QString pattern = ToQString(AppConfig::translationPrefix()) +  "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";
  QRegExp exp(pattern);
  while (langIter.hasNext()) {
    langIter.next();
    QString file = langIter.fileName();
    if (exp.exactMatch(file)) {
      QString languageCode = exp.cap(1);
      QLocale locale(languageCode);
      QString languageString = QLocale::languageToString(locale.language());
      if (locale.language() == QLocale::Chinese) {
        if (languageCode == "zh_TW") {
          languageString = "Chinese (traditional)";
        } else {
          languageString = "Chinese (simplified)";
        }
      }

      languageBox->addItem(QString("%1").arg(languageString), exp.cap(1));
    }
  }
}

void Settings::addStyles(QComboBox *styleBox)
{
  styleBox->addItem("None", "");
  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::stylesheetsPath()), QStringList("*.qss"), QDir::Files);
  while (langIter.hasNext()) {
    langIter.next();
    QString style = langIter.fileName();
    styleBox->addItem(style, style);
  }
}

bool Settings::isNXMHandler(bool *modifyable)
{
  QSettings handlerReg("HKEY_CLASSES_ROOT\\nxm\\shell\\open\\command",
                       QSettings::NativeFormat);

  QString currentExe = handlerReg.value("Default", "").toString().toUtf8().constData();
  QString myExe = QString("\"%1\" ").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).append("\"%1\"");
  if (modifyable != NULL) {
    handlerReg.setValue("Default", currentExe);
    handlerReg.sync();

    *modifyable = handlerReg.status() == QSettings::NoError;
    // QSettings::isWritable returns wrong results...
  }
  return currentExe == myExe;
}


void Settings::setNXMHandlerActive(bool active, bool writable)
{
  QSettings handlerReg("HKEY_CLASSES_ROOT\\nxm\\",
                       QSettings::NativeFormat);

  if (writable) {
    QString myExe = QString("\"%1\" ").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).append("\"%1\"");
    handlerReg.setValue("Default", "URL:NXM Protocol");
    handlerReg.setValue("URL Protocol", "");
    handlerReg.setValue("shell/open/command/Default", active ? myExe : "");
    handlerReg.sync();
  } else {
    Helper::setNXMHandler(GameInfo::instance().getOrganizerDirectory(), active);
  }
}


void Settings::resetDialogs()
{
  m_Settings.beginGroup("DialogChoices");
  QStringList keys = m_Settings.childKeys();
  foreach (QString key, keys) {
    m_Settings.remove(key);
  }

  m_Settings.endGroup();
}


void Settings::query(QWidget *parent)
{
  SettingsDialog dialog(parent);

  connect(&dialog, SIGNAL(resetDialogs()), this, SLOT(resetDialogs()));

  QCheckBox *hideUncheckedBox = dialog.findChild<QCheckBox*>("hideUncheckedBox");
  QCheckBox *forceEnableBox = dialog.findChild<QCheckBox*>("forceEnableBox");
  QComboBox *mechanismBox = dialog.findChild<QComboBox*>("mechanismBox");

  QCheckBox *loginCheckBox = dialog.findChild<QCheckBox*>("loginCheckBox");
  QLineEdit *usernameEdit = dialog.findChild<QLineEdit*>("usernameEdit");
  QLineEdit *passwordEdit = dialog.findChild<QLineEdit*>("passwordEdit");

  QLineEdit *appIDEdit = dialog.findChild<QLineEdit*>("appIDEdit");

  QComboBox *languageBox = dialog.findChild<QComboBox*>("languageBox");
  QComboBox *styleBox = dialog.findChild<QComboBox*>("styleBox");
  QComboBox *logLevelBox = dialog.findChild<QComboBox*>("logLevelBox");
  QCheckBox *handleNXMBox = dialog.findChild<QCheckBox*>("handleNXMBox");

  QLineEdit *downloadDirEdit = dialog.findChild<QLineEdit*>("downloadDirEdit");
  QLineEdit *modDirEdit = dialog.findChild<QLineEdit*>("modDirEdit");
  QLineEdit *cacheDirEdit = dialog.findChild<QLineEdit*>("cacheDirEdit");

  QCheckBox *preferIntegratedCheckBox = dialog.findChild<QCheckBox*>("preferIntegratedBox");
  QCheckBox *preferExternalBox = dialog.findChild<QCheckBox*>("preferExternalBox");
  QCheckBox *quickInstallerBox = dialog.findChild<QCheckBox*>("quickInstallerBox");
  QLineEdit *nmmVersionEdit = dialog.findChild<QLineEdit*>("nmmVersionEdit");

  QListWidget *pluginsList = dialog.findChild<QListWidget*>("pluginsList");

  //
  // set up current settings
  //
  LoadMechanism::EMechanism mechanismID = getLoadMechanism();
  int index = 0;

  if (m_LoadMechanism.isDirectLoadingSupported()) {
    mechanismBox->addItem(QObject::tr("Mod Organizer"), LoadMechanism::LOAD_MODORGANIZER);
    if (mechanismID == LoadMechanism::LOAD_MODORGANIZER) {
      index = mechanismBox->count() - 1;
    }
  }

  if (m_LoadMechanism.isScriptExtenderSupported()) {
    mechanismBox->addItem(QObject::tr("Script Extender"), LoadMechanism::LOAD_SCRIPTEXTENDER);
    if (mechanismID == LoadMechanism::LOAD_SCRIPTEXTENDER) {
      index = mechanismBox->count() - 1;
    }
  }

  if (m_LoadMechanism.isProxyDLLSupported()) {
    mechanismBox->addItem(QObject::tr("Proxy DLL"), LoadMechanism::LOAD_PROXYDLL);
    if (mechanismID == LoadMechanism::LOAD_PROXYDLL) {
      index = mechanismBox->count() - 1;
    }
  }

  mechanismBox->setCurrentIndex(index);

  {
    addLanguages(languageBox);
    int currentID = languageBox->findData(m_Settings.value("Settings/language", QLocale::system().name()).toString());
    if (currentID != -1) {
      languageBox->setCurrentIndex(currentID);
    }
  }

  {
    addStyles(styleBox);
    int currentID = styleBox->findData(m_Settings.value("Settings/style", "").toString());
    if (currentID != -1) {
      styleBox->setCurrentIndex(currentID);
    }
  }

  hideUncheckedBox->setChecked(hideUncheckedPlugins());
  forceEnableBox->setChecked(forceEnableCoreFiles());

  appIDEdit->setText(getSteamAppID());

  if (automaticLoginEnabled()) {
    loginCheckBox->setChecked(true);
    usernameEdit->setText(m_Settings.value("Settings/nexus_username", "").toString());
    passwordEdit->setText(deObfuscate(m_Settings.value("Settings/nexus_password", "").toString()));
  }

  bool registryWritable = false;
  bool nxmHandler = isNXMHandler(&registryWritable);
  handleNXMBox->setChecked(nxmHandler);
  if (!registryWritable) {
    handleNXMBox->setIcon(QIcon(":/MO/gui/locked"));
    handleNXMBox->setToolTip(tr("Administrative rights required to change this."));
  }

  downloadDirEdit->setText(getDownloadDirectory());
  modDirEdit->setText(getModDirectory());
  cacheDirEdit->setText(getCacheDirectory());
  preferIntegratedCheckBox->setChecked(m_Settings.value("Settings/prefer_integrated_installer", false).toBool());
  preferExternalBox->setChecked(m_Settings.value("Settings/prefer_external_browser", false).toBool());
  quickInstallerBox->setChecked(m_Settings.value("Settings/enable_quick_installer", true).toBool());
  nmmVersionEdit->setText(m_Settings.value("Settings/nmm_version", "0.33.1").toString());
  logLevelBox->setCurrentIndex(logLevel());

  foreach (IPlugin *plugin, m_Plugins) {
    QListWidgetItem *listItem = new QListWidgetItem(plugin->name(), pluginsList);
    listItem->setData(Qt::UserRole, QVariant::fromValue((void*)plugin));
    listItem->setData(Qt::UserRole + 1, m_PluginSettings[plugin->name()]);
    pluginsList->addItem(listItem);
  }

  if (dialog.exec() == QDialog::Accepted) {
    //
    // transfer modified settings to configuration file
    //

    m_Settings.setValue("Settings/hide_unchecked_plugins", hideUncheckedBox->checkState() ? true : false);
    m_Settings.setValue("Settings/force_enable_core_files", forceEnableBox->checkState() ? true : false);
    m_Settings.setValue("Settings/load_mechanism", mechanismBox->itemData(mechanismBox->currentIndex()).toInt());
    if (QDir(downloadDirEdit->text()).exists()) {
      m_Settings.setValue("Settings/download_directory", QDir::toNativeSeparators(downloadDirEdit->text()));
    }
    if (!QDir(cacheDirEdit->text()).exists()) {
      QDir().mkpath(cacheDirEdit->text());
    }
    m_Settings.setValue("Settings/cache_directory", QDir::toNativeSeparators(cacheDirEdit->text()));
    if (QDir(modDirEdit->text()).exists()) {
      if ((QDir::fromNativeSeparators(modDirEdit->text()) != QDir::fromNativeSeparators(getModDirectory())) &&
          (QMessageBox::question(NULL, tr("Confirm"), tr("Changing the mod directory affects all your profiles! "
                                                         "Mods not present (or named differently) in the new location will be disabled in all profiles. "
                                                         "There is no way to undo this unless you backed up your profiles manually. Proceed?"),
                                 QMessageBox::Yes | QMessageBox::No))) {
        m_Settings.setValue("Settings/mod_directory", QDir::toNativeSeparators(modDirEdit->text()));
      }
    }
    QString oldLanguage = m_Settings.value("Settings/language", "en_US").toString();
    QString newLanguage = languageBox->itemData(languageBox->currentIndex()).toString();
    if (newLanguage != oldLanguage) {
      m_Settings.setValue("Settings/language", newLanguage);
      emit languageChanged(newLanguage);
    }

    QString oldStyle = m_Settings.value("Settings/style", "").toString();
    QString newStyle = styleBox->itemData(styleBox->currentIndex()).toString();
    if (oldStyle != newStyle) {
      m_Settings.setValue("Settings/style", newStyle);
      emit styleChanged(newStyle);
    }

    m_Settings.setValue("Settings/log_level", logLevelBox->currentIndex());

    if (appIDEdit->text() != ToQString(GameInfo::instance().getSteamAPPId())) {
      m_Settings.setValue("Settings/app_id", appIDEdit->text());
    } else {
      m_Settings.remove("Settings/app_id");
    }
    if (loginCheckBox->isChecked()) {
      m_Settings.setValue("Settings/nexus_login", true);
      m_Settings.setValue("Settings/nexus_username", usernameEdit->text());
      m_Settings.setValue("Settings/nexus_password", obfuscate(passwordEdit->text()));
    } else {
      m_Settings.setValue("Settings/nexus_login", false);
      m_Settings.remove("Settings/nexus_username");
      m_Settings.remove("Settings/nexus_password");
    }
    if (nxmHandler != handleNXMBox->isChecked()) {
      setNXMHandlerActive(handleNXMBox->isChecked(), registryWritable);
    }
    m_Settings.setValue("Settings/prefer_integrated_installer", preferIntegratedCheckBox->isChecked());
    m_Settings.setValue("Settings/prefer_external_browser", preferExternalBox->isChecked());
    m_Settings.setValue("Settings/enable_quick_installer", quickInstallerBox->isChecked());

    m_Settings.setValue("Settings/nmm_version", nmmVersionEdit->text());

    // transfer plugin settings to in-memory structure
    for (int i = 0; i < pluginsList->count(); ++i) {
      QListWidgetItem *item = pluginsList->item(i);
      m_PluginSettings[item->text()] = item->data(Qt::UserRole + 1).toMap();
    }
    // store plugin settings on disc
    for (auto iterPlugins = m_PluginSettings.begin(); iterPlugins != m_PluginSettings.end(); ++iterPlugins) {
      for (auto iterSettings = iterPlugins->begin(); iterSettings != iterPlugins->end(); ++iterSettings) {
        m_Settings.setValue("Plugins/" + iterPlugins.key() + "/" + iterSettings.key(), iterSettings.value());
      }
    }
  }
}