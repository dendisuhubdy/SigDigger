//
//    Library.cpp: C-level Suscan API initialization
//    Copyright (C) 2018 Gonzalo José Carracedo Carballal
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//

#include <Suscan/Library.h>
#include <Suscan/MultitaskController.h>
#include <analyzer/version.h>
#include <QtGui>

using namespace Suscan;

// Singleton initialization
Singleton *Singleton::instance = nullptr;
Logger    *Singleton::logger   = nullptr;

uint
Suscan::qHash(const Suscan::Source::Device &dev)
{
  return
      ::qHash(dev.getDesc().c_str())
    ^ ::qHash(dev.getDriver().c_str())
    ^ ::qHash(dev.isRemote());
}

Singleton::Singleton()
{
  this->codecs_initd = false;
  this->sources_initd = false;
  this->estimators_initd = false;
  this->spectrum_sources_initd = false;
  this->inspectors_initd = false;

  this->backgroundTaskController = new MultitaskController;

  // Define some read-only units. We may let the user add customized
  // units too.

  this->registerSpectrumUnit("dBFS",     1.0, 0.0f);
  this->registerSpectrumUnit("dBK",      1.0, -228.60f);
  this->registerSpectrumUnit("dBW/Hz",   1.0, 0.0f);
  this->registerSpectrumUnit("dBm/Hz",   1.0, -30.0f);

  this->registerSpectrumUnit("dBJy",     1.0, 0.0f);

  // The zero point of the AB magnitude scale is at 3631 Jy. This is,
  // at 35.6 dB above the zero point of the dBJy scale. Since 1 mag = -4 dB,
  // the zero point of the scale exactly at -8.9 mag w.r.t the zero point of the
  // dBJy scale.
  this->registerSpectrumUnit("mag (AB)", -4.0, -2.5f * SU_LOG(3631.f));

  this->logger = Logger::getInstance();
}

Singleton::~Singleton()
{
  this->killBackgroundTaskController();
}

Singleton *
Singleton::get_instance(void)
{
  if (Singleton::instance == nullptr) {
    try {
      Singleton::instance = new Singleton();
    } catch (std::bad_alloc&) {
      throw Exception("Failed to allocate Suscan's singleton");
    }
  }

  return Singleton::instance;
}

std::string
Singleton::sigutilsVersion(void)
{
  return std::string(sigutils_api_version())
      + " (" + std::string(sigutils_pkgversion()) + ")";
}

std::string
Singleton::suscanVersion(void)
{
  return std::string(suscan_api_version())
      + " (" + std::string(suscan_pkgversion()) + ")";
}

// Initialization methods
void
Singleton::init_codecs(void)
{
  if (!this->codecs_initd)
    SU_ATTEMPT(suscan_codec_class_register_builtin());
}

static SUBOOL
walk_all_sources(suscan_source_config_t *config, void *privdata)
{
  Singleton *instance = static_cast<Singleton *>(privdata);

  instance->registerSourceConfig(config);

  return SU_TRUE;
}

static SUBOOL
walk_all_devices(const suscan_source_device_t *device, unsigned int, void *privdata)
{
  Singleton *instance = static_cast<Singleton *>(privdata);

  instance->registerSourceDevice(device);

  return SU_TRUE;
}

static SUBOOL
walk_all_remote_devices(
    void *privdata,
    const suscan_source_device_t *,
    const suscan_source_config_t *config)
{
  Singleton *instance = static_cast<Singleton *>(privdata);

  instance->registerNetworkProfile(config);

  return SU_TRUE;
}

void
Singleton::init_sources(void)
{
  if (!this->sources_initd) {
    SU_ATTEMPT(suscan_init_sources());
    suscan_source_config_walk(walk_all_sources, static_cast<void *>(this));
    suscan_source_device_walk(walk_all_devices, static_cast<void *>(this));
    this->sources_initd = true;
  }
}

void
Singleton::init_estimators(void)
{
  if (!this->estimators_initd) {
    SU_ATTEMPT(suscan_init_estimators());
    this->estimators_initd = true;
  }
}

void
Singleton::init_spectrum_sources(void)
{
  if (!this->spectrum_sources_initd) {
    SU_ATTEMPT(suscan_init_spectsrcs());
    this->spectrum_sources_initd = true;
  }
}

void
Singleton::init_inspectors(void)
{
  if (!this->inspectors_initd) {
    SU_ATTEMPT(suscan_init_inspectors());
    this->inspectors_initd = true;
  }
}

bool
Singleton::haveAutoGain(std::string const &name)
{
  for (auto p = this->autoGains.begin();
       p != this->autoGains.end();
       ++p) {
    try {
      if (p->getField("name").value() == name)
        return true;
    } catch (Suscan::Exception const &) {

    }
  }

  return false;
}

bool
Singleton::haveFAT(std::string const &name)
{
  for (auto p = this->FATs.begin();
       p != this->FATs.end();
       ++p) {
    try {
      if (p->getField("name").value() == name)
        return true;
    } catch (Suscan::Exception const &) {

    }
  }

  return false;
}

bool
Singleton::havePalette(std::string const &name)
{
  for (auto p = this->palettes.begin();
       p != this->palettes.end();
       ++p) {
    try {
      if (p->getField("name").value() == name)
        return true;
    } catch (Suscan::Exception const &) {

    }
  }

  return false;
}

void
Singleton::init_palettes(void)
{
  unsigned int i, count;
  ConfigContext ctx("palettes");
  Object list = ctx.listObject();

  ctx.setSave(false);

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      if (!this->havePalette(list[i].getField("name").value()))
        this->palettes.push_back(list[i]);
    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::init_autogains(void)
{
  unsigned int i, count;
  ConfigContext ctx("autogains");
  Object list = ctx.listObject();

  ctx.setSave(false);

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      if (!this->haveAutoGain(list[i].getField("name").value()))
        this->autoGains.push_back(list[i]);
    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::init_fats(void)
{
  unsigned int i, count;
  ConfigContext ctx("frequency_allocations");
  Object list = ctx.listObject();

  ctx.setSave(false);

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      if (!this->haveFAT(list[i].getField("name").value()))
        this->FATs.push_back(list[i]);
    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::init_bookmarks(void)
{
  unsigned int i, count;
  ConfigContext ctx("bookmarks");
  Object list = ctx.listObject();
  qreal freq;

  ctx.setSave(true);

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      Bookmark bm;
      std::string frequency = list[i].getField("frequency").value();

      bm.info.name = QString::fromStdString(list[i].getField("name").value());
      bm.info.color = QColor(QString::fromStdString(list[i].getField("color").value()));

      try {
        // try parse extended informations
        std::string lowFreqCut = list[i].getField("low_freq_cut").value();
        std::string highFreqCut = list[i].getField("high_freq_cut").value();
        bm.info.modulation = QString::fromStdString(list[i].getField("modulation").value());

        (void) sscanf(lowFreqCut.c_str(), "%d", &bm.info.lowFreqCut);
        (void) sscanf(highFreqCut.c_str(), "%d", &bm.info.highFreqCut);

      } catch (Suscan::Exception const &) { }

      if (sscanf(frequency.c_str(), "%lg", &freq) == 1 && bm.info.name.size() > 0) {
        bm.info.frequency = static_cast<qint64>(freq);
        bm.entry = static_cast<int>(i);
        this->bookmarks[bm.info.frequency] = bm;
      }

    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::refreshDevices(void)
{
  this->devices.clear();
  suscan_source_device_walk(walk_all_devices, static_cast<void *>(this));
}

void
Singleton::refreshNetworkProfiles(void)
{
  this->networkProfiles.clear();
  suscan_discovered_remote_device_walk(
        walk_all_remote_devices,
        static_cast<void *>(this));
}

void
Singleton::detect_devices(void)
{
  suscan_source_detect_devices();
  this->refreshDevices();
}

void
Singleton::init_ui_config(void)
{
  unsigned int i, count;
  ConfigContext ctx("uiconfig");

  Object list = ctx.listObject();

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      this->uiConfig.push_back(list[i]);
    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::init_recent_list(void)
{
  unsigned int i, count;
  ConfigContext ctx("recent");

  Object list = ctx.listObject();

  count = list.length();

  for (i = 0; i < count; ++i) {
    try {
      if (list[i].getType() == SUSCAN_OBJECT_TYPE_FIELD) {
        this->recentProfiles.push_back(list[i].value());
      }
    } catch (Suscan::Exception const &) { }
  }
}

void
Singleton::syncRecent(void)
{
  ConfigContext ctx("recent");
  Object list = ctx.listObject();

  list.clear();

  for (auto p : this->recentProfiles) {
    try {
      list.append(Object::makeField(p));
    } catch (Suscan::Exception const &) {
      // Don't even bother to warn
    }
  }
}

void
Singleton::syncUI(void)
{
  unsigned int i, count;
  ConfigContext ctx("uiconfig");
  Object list = ctx.listObject();

  count = static_cast<unsigned int>(this->uiConfig.size());

  // Sync all modified configurations
  for (i = 0; i < count; ++i) {
    if (!this->uiConfig[i].isBorrowed()) {
      try {
        list.put(this->uiConfig[i], i);
      } catch (Suscan::Exception const &) {
        list.append(this->uiConfig[i]);
      }
    }
  }
}

void
Singleton::syncBookmarks(void)
{
  ConfigContext ctx("bookmarks");
  Object list = ctx.listObject();

  // Sync all modified configurations
  for (auto p : this->bookmarks.keys()) {
    if (this->bookmarks[p].entry == -1) {
      try {
        Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

        obj.set("name", this->bookmarks[p].info.name.toStdString());
        obj.set("frequency", static_cast<double>(this->bookmarks[p].info.frequency));
        obj.set("color", this->bookmarks[p].info.color.name().toStdString());
        obj.set("low_freq_cut", this->bookmarks[p].info.lowFreqCut);
        obj.set("high_freq_cut", this->bookmarks[p].info.highFreqCut);
        obj.set("modulation", this->bookmarks[p].info.modulation.toStdString());

        list.append(std::move(obj));
      } catch (Suscan::Exception const &) {
      }
    }
  }
}

void
Singleton::killBackgroundTaskController(void)
{
  if (this->backgroundTaskController != nullptr) {
    delete this->backgroundTaskController;
    this->backgroundTaskController = nullptr;
  }
}

void
Singleton::sync(void)
{
  this->syncRecent();
  this->syncUI();
  this->syncBookmarks();
}

// Singleton methods
void
Singleton::registerSourceConfig(suscan_source_config_t *config)
{
  const char *label = suscan_source_config_get_label(config);

  if (label == nullptr)
    label = "(Null profile)";

  this->profiles[label] = Suscan::Source::Config(config);
}

void
Singleton::registerNetworkProfile(const suscan_source_config_t *config)
{
  QString name = QString(suscan_source_config_get_label(config));

  this->networkProfiles[name] = Suscan::Source::Config::wrap(
        suscan_source_config_clone(config));
}

MultitaskController *
Singleton::getBackgroundTaskController(void) const
{
  return this->backgroundTaskController;
}

ConfigMap::const_iterator
Singleton::getFirstProfile(void) const
{
  return this->profiles.begin();
}

ConfigMap::const_iterator
Singleton::getLastProfile(void) const
{
  return this->profiles.end();
}

Suscan::Source::Config *
Singleton::getProfile(std::string const &name)
{
  if (this->profiles.find(name) == this->profiles.end())
    return nullptr;

  return &this->profiles[name];
}

void
Singleton::saveProfile(Suscan::Source::Config const &profile)
{
  this->profiles[profile.label()] = profile;

  SU_ATTEMPT(
        suscan_source_config_register(
          this->profiles[profile.label()].instance));
}

void
Singleton::removeBookmark(qint64 freq)
{
  if (this->bookmarks.find(freq) != this->bookmarks.end()) {
    Bookmark bm = this->bookmarks[freq];
    this->bookmarks.remove(freq);

    if (bm.entry != -1) {
      ConfigContext ctx("bookmarks");
      Object list = ctx.listObject();
      list.remove(static_cast<unsigned>(bm.entry));
    }
  }
}

void
Singleton::replaceBookmark(BookmarkInfo const& info)
{
  Bookmark bm;

  bm.info = info;

  this->removeBookmark(info.frequency);
  this->bookmarks[info.frequency] = bm;
}

bool
Singleton::registerBookmark(BookmarkInfo const& info)
{
  if (this->bookmarks.find(info.frequency) != this->bookmarks.end())
    return false;

  Bookmark bm;

  bm.info = info;
  this->bookmarks[info.frequency] = bm;

  return true;
}

bool
Singleton::registerSpectrumUnit(
    std::string const &name,
    float dBPerUnit,
    float zeroPoint)
{
  if (this->spectrumUnits.find(name) != this->spectrumUnits.end())
    return false;

  SpectrumUnit su;

  su.name      = name;
  su.dBPerUnit = dBPerUnit;
  su.zeroPoint = zeroPoint;

  this->spectrumUnits[name] = su;

  return true;
}

void
Singleton::replaceSpectrumUnit(
    std::string const &name,
    float dBPerUnit,
    float zeroPoint)
{
  SpectrumUnit su;

  su.name      = name;
  su.dBPerUnit = dBPerUnit;
  su.zeroPoint = zeroPoint;

  this->removeSpectrumUnit(name);
  this->spectrumUnits[name] = su;
}

void
Singleton::removeSpectrumUnit(std::string const &name)
{
  if (this->spectrumUnits.find(name) != this->spectrumUnits.end()) {
    SpectrumUnit bm = this->spectrumUnits[name];
    this->spectrumUnits.remove(name);
  }
}

void
Singleton::registerSourceDevice(const suscan_source_device_t *dev)
{
  this->devices.push_back(Source::Device(dev, 0));
}

std::vector<Source::Device>::const_iterator
Singleton::getFirstDevice(void) const
{
  return this->devices.begin();
}

std::vector<Source::Device>::const_iterator
Singleton::getLastDevice(void) const
{
  return this->devices.end();
}

std::vector<Object>::const_iterator
Singleton::getFirstPalette(void) const
{
  return this->palettes.begin();
}

std::vector<Object>::const_iterator
Singleton::getLastPalette(void) const
{
  return this->palettes.end();
}

std::vector<Object>::const_iterator
Singleton::getFirstAutoGain(void) const
{
  return this->autoGains.begin();
}

std::vector<Object>::const_iterator
Singleton::getLastAutoGain(void) const
{
  return this->autoGains.end();
}

std::vector<Object>::iterator
Singleton::getFirstUIConfig(void)
{
  return this->uiConfig.begin();
}

std::vector<Object>::iterator
Singleton::getLastUIConfig(void)
{
  return this->uiConfig.end();
}

std::vector<Object>::const_iterator
Singleton::getFirstFAT(void) const
{
  return this->FATs.begin();
}

std::vector<Object>::const_iterator
Singleton::getLastFAT(void) const
{
  return this->FATs.end();
}

void
Singleton::putUIConfig(unsigned int pos, Object &&rv)
{
  if (pos >= this->uiConfig.size())
    this->uiConfig.resize(pos + 1);

  this->uiConfig[pos] = std::move(rv);
}

const Source::Device *
Singleton::getDeviceAt(unsigned int index) const
{
  if (index < this->devices.size())
    return &this->devices[index];

  return nullptr;
}

std::list<std::string>::const_iterator
Singleton::getFirstRecent(void) const
{
  return this->recentProfiles.cbegin();
}

std::list<std::string>::const_iterator
Singleton::getLastRecent(void) const
{
  return this->recentProfiles.cend();
}

QMap<qint64,Bookmark> const &
Singleton::getBookmarkMap(void) const
{
  return this->bookmarks;
}

QMap<qint64,Bookmark>::const_iterator
Singleton::getFirstBookmark(void) const
{
  return this->bookmarks.cbegin();
}

QMap<qint64,Bookmark>::const_iterator
Singleton::getLastBookmark(void) const
{
  return this->bookmarks.cend();
}

QMap<qint64,Bookmark>::const_iterator
Singleton::getBookmarkFrom(qint64 freq) const
{
  return this->bookmarks.lowerBound(freq);
}

QMap<std::string, SpectrumUnit> const &
Singleton::getSpectrumUnitMap(void) const
{
  return this->spectrumUnits;
}

QMap<std::string, SpectrumUnit>::const_iterator
Singleton::getFirstSpectrumUnit(void) const
{
  return this->spectrumUnits.cbegin();
}

QMap<std::string, SpectrumUnit>::const_iterator
Singleton::getLastSpectrumUnit(void) const
{
  return this->spectrumUnits.cend();
}

QMap<std::string, SpectrumUnit>::const_iterator
Singleton::getSpectrumUnitFrom(std::string const &name) const
{
  return this->spectrumUnits.lowerBound(name);
}


QHash<QString, Source::Config> const &
Singleton::getNetworkProfileMap(void) const
{
  return this->networkProfiles;
}

QHash<QString, Source::Config>::const_iterator
Singleton::getFirstNetworkProfile(void) const
{
  return this->networkProfiles.cbegin();
}

QHash<QString, Source::Config>::const_iterator
Singleton::getLastNetworkProfile(void) const
{
  return this->networkProfiles.cend();
}

QHash<QString, Source::Config>::const_iterator
Singleton::getNetworkProfileFrom(QString const &name) const
{
  return this->networkProfiles.constFind(name);
}

bool
Singleton::notifyRecent(std::string const &name)
{
  bool found = false;

  found = this->removeRecent(name);

  this->recentProfiles.push_front(name);

  return found;
}

bool
Singleton::removeRecent(std::string const &name)
{
  bool found = false;

  for (auto p = this->getFirstRecent(); p != this->getLastRecent(); ++p) {
    if (*p == name) {
      auto current = p;
      --current;

      this->recentProfiles.erase(p);

      p = current;
      found = true;
      continue;
    }
  }

  return found;
}

void
Singleton::clearRecent(void)
{
  this->recentProfiles.clear();
}
