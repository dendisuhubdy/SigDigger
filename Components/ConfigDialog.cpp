//
//    ConfigDialog.cpp: Configuration dialog window
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

#include <QFileDialog>
#include <QMessageBox>

#include <Suscan/Library.h>
#include <SuWidgetsHelpers.h>
#include "ConfigDialog.h"

#define SIGDIGGER_CONFIG_DIALOG_MIN_DEVICE_FREQ 0
#define SIGDIGGER_CONFIG_DIALOG_MAX_DEVICE_FREQ 7.5e9

using namespace SigDigger;

Q_DECLARE_METATYPE(Suscan::Source::Config); // Unicorns
Q_DECLARE_METATYPE(Suscan::Source::Device); // More unicorns

void
ConfigDialog::populateCombos(void)
{
  Suscan::Singleton *sus = Suscan::Singleton::get_instance();

  this->ui->profileCombo->clear();
  this->ui->deviceCombo->clear();
  this->ui->remoteDeviceCombo->clear();

  for (auto i = sus->getFirstProfile(); i != sus->getLastProfile(); ++i)
      this->ui->profileCombo->addItem(
            QString::fromStdString(i->first),
            QVariant::fromValue(i->second));

  // Populate local devices only
  for (auto i = sus->getFirstDevice(); i != sus->getLastDevice(); ++i)
    if (i->isAvailable() && !i->isRemote())
      this->ui->deviceCombo->addItem(
          QString::fromStdString(i->getDesc()),
          QVariant::fromValue<long>(i - sus->getFirstDevice()));

  if (this->ui->deviceCombo->currentIndex() == -1)
    this->ui->deviceCombo->setCurrentIndex(0);

  // Network devices are traversed here.
  for (
       auto i = sus->getFirstNetworkProfile();
       i != sus->getLastNetworkProfile();
       ++i)
    this->ui->remoteDeviceCombo->addItem(i->label().c_str());

  if (this->ui->remoteDeviceCombo->currentIndex() != -1)
    this->ui->remoteDeviceCombo->setCurrentIndex(0);

  this->onDeviceChanged(this->ui->deviceCombo->currentIndex());
}

void
ConfigDialog::refreshUiState(void)
{
  int analyzerTypeIndex = this->ui->analyzerTypeCombo->currentIndex();
  bool netProfile = this->ui->useNetworkProfileRadio->isChecked();

  this->ui->analyzerParamsStackedWidget->setCurrentIndex(analyzerTypeIndex);

  if (!this->remoteSelected()) {
    /* Local analyzer */
    if (this->ui->sdrRadio->isChecked()) {
      this->ui->sdrFrame->setEnabled(true);
      this->ui->fileFrame->setEnabled(false);
      this->ui->sampRateStack->setCurrentIndex(0);
      this->ui->ppmSpinBox->setEnabled(true);
    } else {
      this->ui->sdrFrame->setEnabled(false);
      this->ui->fileFrame->setEnabled(true);
      this->ui->ppmSpinBox->setEnabled(false);
      this->ui->sampRateStack->setCurrentIndex(1);
    }
  } else {
    /* Remote analyzer */
    this->ui->sampRateStack->setCurrentIndex(1);

    if (this->ui->remoteDeviceCombo->count() == 0) {
      if (netProfile)
        netProfile = false;
      this->ui->useNetworkProfileRadio->setChecked(false);
      this->ui->useHostPortRadio->setChecked(true);
      this->ui->useNetworkProfileRadio->setEnabled(false);
    } else {
      this->ui->useNetworkProfileRadio->setEnabled(true);
    }

    this->ui->hostEdit->setEnabled(!netProfile);
    this->ui->portEdit->setEnabled(!netProfile);
    this->ui->remoteDeviceCombo->setEnabled(netProfile);
    this->ui->ppmSpinBox->setEnabled(true);
  }

  this->setSelectedSampleRate(this->profile.getSampleRate());
  this->refreshTrueSampleRate();
}

void
ConfigDialog::refreshAntennas(void)
{
  populateAntennaCombo(this->profile, this->ui->antennaCombo);
}

void
ConfigDialog::refreshSampRates(void)
{
  Suscan::Source::Device device = this->profile.getDevice();

  this->ui->sampleRateCombo->clear();

  for (
       auto p = device.getFirstSampRate();
       p != device.getLastSampRate();
       ++p) {
    this->ui->sampleRateCombo->addItem(
          getSampRateString(*p),
          QVariant::fromValue<double>(*p));
  }
}

#define APSTOREF(widget, field) \
  this->ui->widget->setText(QString::number(static_cast<qreal>(\
    this->analyzerParams.field)))
#define APSTOREI(widget, field) \
  this->ui->widget->setText(QString::number(this->analyzerParams.field))
#define APSAVEF(widget, field) \
  this->analyzerParams.field = this->ui->widget->text().toFloat()
#define APSAVEI(widget, field) \
  this->analyzerParams.field = this->ui->widget->text().toUInt()

void
ConfigDialog::saveAnalyzerParams(void)
{
  APSAVEF(spectAvgAlphaEdit, spectrumAvgAlpha);
  APSAVEF(sLevelAvgAlphaEdit, sAvgAlpha);
  APSAVEF(nLevelAvgAlphaEdit, nAvgAlpha);
  APSAVEF(snrThresholdEdit, snr);
  APSAVEF(spectrumRefreshEdit, psdUpdateInterval);
  APSAVEF(channelRefreshEdit, channelUpdateInterval);
  APSAVEI(fftSizeEdit, windowSize);

  this->analyzerParams.psdUpdateInterval *= 1e-3f;
  this->analyzerParams.channelUpdateInterval *= 1e-3f;

  if (this->ui->rectangularRadio->isChecked())
    this->analyzerParams.windowFunction = Suscan::AnalyzerParams::NONE;
  else if (this->ui->hammingRadio->isChecked())
    this->analyzerParams.windowFunction = Suscan::AnalyzerParams::HAMMING;
  else if (this->ui->hannRadio->isChecked())
    this->analyzerParams.windowFunction = Suscan::AnalyzerParams::HANN;
  else if (this->ui->flatTopRadio->isChecked())
    this->analyzerParams.windowFunction = Suscan::AnalyzerParams::FLAT_TOP;
  else if (this->ui->blackmannHarrisRadio->isChecked())
    this->analyzerParams.windowFunction = Suscan::AnalyzerParams::BLACKMANN_HARRIS;
}

void
ConfigDialog::refreshAnalyzerParamsUi(void)
{
  this->analyzerParams.psdUpdateInterval *= 1e3f;
  this->analyzerParams.channelUpdateInterval *= 1e3f;

  APSTOREF(spectAvgAlphaEdit, spectrumAvgAlpha);
  APSTOREF(sLevelAvgAlphaEdit, sAvgAlpha);
  APSTOREF(nLevelAvgAlphaEdit, nAvgAlpha);
  APSTOREF(snrThresholdEdit, snr);
  APSTOREF(spectrumRefreshEdit, psdUpdateInterval);
  APSTOREF(channelRefreshEdit, channelUpdateInterval);
  APSTOREI(fftSizeEdit, windowSize);

  this->analyzerParams.psdUpdateInterval *= 1e-3f;
  this->analyzerParams.channelUpdateInterval *= 1e-3f;

  switch (this->analyzerParams.windowFunction) {
    case Suscan::AnalyzerParams::NONE:
      this->ui->rectangularRadio->setChecked(true);
      break;

    case Suscan::AnalyzerParams::HAMMING:
      this->ui->hammingRadio->setChecked(true);
      break;

    case Suscan::AnalyzerParams::HANN:
      this->ui->hannRadio->setChecked(true);
      break;

    case Suscan::AnalyzerParams::FLAT_TOP:
      this->ui->flatTopRadio->setChecked(true);
      break;

    case Suscan::AnalyzerParams::BLACKMANN_HARRIS:
      this->ui->blackmannHarrisRadio->setChecked(true);
      break;
  }
}

void
ConfigDialog::refreshFrequencyLimits(void)
{
  SUFREQ lnbFreq = this->ui->lnbSpinBox->value();
  SUFREQ devMinFreq = SIGDIGGER_CONFIG_DIALOG_MIN_DEVICE_FREQ;
  SUFREQ devMaxFreq = SIGDIGGER_CONFIG_DIALOG_MAX_DEVICE_FREQ;

  if (this->profile.getType() == SUSCAN_SOURCE_TYPE_FILE) {
    devMinFreq = SIGDIGGER_MIN_RADIO_FREQ;
    devMaxFreq = SIGDIGGER_MAX_RADIO_FREQ;
  } else {
    const Suscan::Source::Device *dev = &(this->profile.getDevice());

    if (dev != nullptr) {
      devMinFreq = dev->getMinFreq();
      devMaxFreq = dev->getMaxFreq();
    }
  }
  // DEVFREQ = FREQ - LNB

  this->ui->frequencySpinBox->setMinimum(devMinFreq + lnbFreq);
  this->ui->frequencySpinBox->setMaximum(devMaxFreq + lnbFreq);
}

#define CCREFRESH(widget, field) this->ui->widget->setColor(this->colors.field)
#define CCSAVE(widget, field) this->ui->widget->getColor(this->colors.field)

void
ConfigDialog::saveColors(void)
{
  CCSAVE(lcdFgColor, lcdForeground);
  CCSAVE(lcdBgColor, lcdBackground);
  CCSAVE(spectrumFgColor, spectrumForeground);
  CCSAVE(spectrumBgColor, spectrumBackground);
  CCSAVE(spectrumAxesColor, spectrumAxes);
  CCSAVE(spectrumTextColor, spectrumText);
  CCSAVE(constellationFgColor, constellationForeground);
  CCSAVE(constellationBgColor, constellationBackground);
  CCSAVE(constellationAxesColor, constellationAxes);
  CCSAVE(transitionFgColor, transitionForeground);
  CCSAVE(transitionBgColor, transitionBackground);
  CCSAVE(transitionAxesColor, transitionAxes);
  CCSAVE(histogramFgColor, histogramForeground);
  CCSAVE(histogramBgColor, histogramBackground);
  CCSAVE(histogramAxesColor, histogramAxes);
  CCSAVE(histogramModelColor, histogramModel);
  CCSAVE(symViewLoColor, symViewLow);
  CCSAVE(symViewHiColor, symViewHigh);
  CCSAVE(symViewBgColor, symViewBackground);
  CCSAVE(selectionColor, selection);
  CCSAVE(filterBoxColor, filterBox);
}

void
ConfigDialog::refreshColorUi(void)
{
  CCREFRESH(lcdFgColor, lcdForeground);
  CCREFRESH(lcdBgColor, lcdBackground);
  CCREFRESH(spectrumFgColor, spectrumForeground);
  CCREFRESH(spectrumBgColor, spectrumBackground);
  CCREFRESH(spectrumAxesColor, spectrumAxes);
  CCREFRESH(spectrumTextColor, spectrumText);
  CCREFRESH(constellationFgColor, constellationForeground);
  CCREFRESH(constellationBgColor, constellationBackground);
  CCREFRESH(constellationAxesColor, constellationAxes);
  CCREFRESH(transitionFgColor, transitionForeground);
  CCREFRESH(transitionBgColor, transitionBackground);
  CCREFRESH(transitionAxesColor, transitionAxes);
  CCREFRESH(histogramFgColor, histogramForeground);
  CCREFRESH(histogramBgColor, histogramBackground);
  CCREFRESH(histogramAxesColor, histogramAxes);
  CCREFRESH(histogramModelColor, histogramModel);
  CCREFRESH(symViewLoColor, symViewLow);
  CCREFRESH(symViewHiColor, symViewHigh);
  CCREFRESH(symViewBgColor, symViewBackground);
  CCREFRESH(selectionColor, selection);
  CCREFRESH(filterBoxColor, filterBox);
}

void
ConfigDialog::saveGuiConfigUi()
{
  this->guiConfig.useLMBdrag = this->ui->reverseDragBehaviorCheck->isChecked();
}

void
ConfigDialog::refreshGuiConfigUi()
{
  this->ui->reverseDragBehaviorCheck->setChecked(this->guiConfig.useLMBdrag);
}

QString
ConfigDialog::getSampRateString(qreal trueRate)
{
  QString rateText;

  if (trueRate < 1e3)
    rateText = QString::number(trueRate) + " sps";
  else if (trueRate < 1e6)
    rateText = QString::number(trueRate * 1e-3) + " ksps";
  else if (trueRate < 1e9)
    rateText = QString::number(trueRate * 1e-6) + " Msps";

  return rateText;
}

void
ConfigDialog::refreshTrueSampleRate(void)
{
  float step = SU_POW(10., SU_FLOOR(SU_LOG(this->profile.getSampleRate())));
  QString rateText;
  qreal trueRate = static_cast<qreal>(this->getSelectedSampleRate())
      / this->ui->decimationSpin->value();
  if (step >= 10.f)
    step /= 10.f;

  this->ui->trueRateLabel->setText(getSampRateString(trueRate));
}

void
ConfigDialog::refreshAnalyzerTypeUi(void)
{
  if (this->profile.getInterface() == SUSCAN_SOURCE_LOCAL_INTERFACE) {
    this->ui->analyzerTypeCombo->setCurrentIndex(0);
  } else {
    this->ui->analyzerTypeCombo->setCurrentIndex(1);
  }

  this->ui->analyzerParamsStackedWidget->setCurrentIndex(
        this->ui->analyzerTypeCombo->currentIndex());
}

int
ConfigDialog::findRemoteProfileIndex(void)
{
  return this->ui->remoteDeviceCombo->findText(this->profile.label().c_str());
}

void
ConfigDialog::refreshProfileUi(void)
{
  Suscan::Singleton *sus = Suscan::Singleton::get_instance();

  for (auto i = 0; i < this->ui->profileCombo->count(); ++i)
    if (this->ui->profileCombo->itemText(i).toStdString() ==
        this->profile.label()) {
      this->ui->profileCombo->setCurrentIndex(i);
      break;
    }

  this->refreshSampRates();

  this->ui->decimationSpin->setValue(
        static_cast<int>(this->profile.getDecimation()));

  switch (this->profile.getType()) {
    case SUSCAN_SOURCE_TYPE_SDR:
      this->ui->sdrRadio->setChecked(true);
      this->ui->sampRateStack->setCurrentIndex(0);
      break;

    case SUSCAN_SOURCE_TYPE_FILE:
      this->ui->fileRadio->setChecked(true);
      this->ui->sampRateStack->setCurrentIndex(1);
      break;
  }

  this->setSelectedSampleRate(this->profile.getSampleRate());

  this->ui->iqBalanceCheck->setChecked(this->profile.getIQBalance());
  this->ui->removeDCCheck->setChecked(this->profile.getDCRemove());
  this->ui->loopCheck->setChecked(this->profile.getLoop());

  this->ui->ppmSpinBox->setValue(
        static_cast<double>(this->profile.getPPM()));

  this->ui->bandwidthSpinBox->setValue(
        static_cast<double>(this->profile.getBandwidth()));

  switch (this->profile.getFormat()) {
    case SUSCAN_SOURCE_FORMAT_AUTO:
      this->ui->formatCombo->setCurrentIndex(0);
      break;

    case SUSCAN_SOURCE_FORMAT_RAW_FLOAT32:
      this->ui->formatCombo->setCurrentIndex(1);
      break;

    case SUSCAN_SOURCE_FORMAT_RAW_UNSIGNED8:
      this->ui->formatCombo->setCurrentIndex(2);
      break;

    case SUSCAN_SOURCE_FORMAT_WAV:
      this->ui->formatCombo->setCurrentIndex(3);
      break;
  }

  this->ui->pathEdit->setText(QString::fromStdString(this->profile.getPath()));

  this->refreshAnalyzerTypeUi();

  if (this->profile.getInterface() == SUSCAN_SOURCE_LOCAL_INTERFACE) {
    // Set local analyzer interface
    for (auto i = sus->getFirstDevice(); i != sus->getLastDevice(); ++i) {
      if (i->equals(this->profile.getDevice())) {
        int index = this->ui->deviceCombo->findData(
              QVariant::fromValue(
                static_cast<long>(i - sus->getFirstDevice())));
        if (index != -1) {
          this->ui->deviceCombo->setCurrentIndex(index);
          this->savedLocalDeviceIndex = index;
        }

        break;
      }
    }

    if (this->ui->deviceCombo->currentIndex() == -1)
      this->ui->deviceCombo->setCurrentIndex(0);
  } else {
    const char *val;
    int index;
    // Set remote analyzer interface
    val = this->profile.getParam("host").c_str();
    this->ui->hostEdit->setText(val);

    try {
      val = this->profile.getParam("port").c_str();
      this->ui->portEdit->setValue(std::stoi(val));
    } catch (std::invalid_argument &) {
      this->ui->portEdit->setValue(28001);
    }

    val = this->profile.getParam("user").c_str();
    this->ui->userEdit->setText(val);


    val = this->profile.getParam("password").c_str();
    this->ui->passEdit->setText(val);

    this->ui->deviceCombo->setCurrentIndex(-1);

    index = this->findRemoteProfileIndex();
    if (index != -1) {
      this->ui->useNetworkProfileRadio->setChecked(true);
      this->ui->useHostPortRadio->setChecked(false);
      this->ui->remoteDeviceCombo->setCurrentIndex(index);
    } else {
      this->ui->useHostPortRadio->setChecked(true);
      this->ui->useNetworkProfileRadio->setChecked(false);
    }
  }

  this->ui->lnbSpinBox->setValue(this->profile.getLnbFreq());
  this->ui->frequencySpinBox->setValue(this->profile.getFreq());
  this->refreshFrequencyLimits();
  this->refreshUiState();
  this->refreshAntennas();
  this->refreshTrueSampleRate();
}

void
ConfigDialog::refreshUi(void)
{
  this->refreshing = true;

  this->refreshColorUi();
  this->refreshProfileUi();
  this->refreshGuiConfigUi();

  this->refreshing = false;
}


void
ConfigDialog::saveProfile()
{
  this->profile.setType(
        this->ui->sdrRadio->isChecked()
        ? SUSCAN_SOURCE_TYPE_SDR
        : SUSCAN_SOURCE_TYPE_FILE);

  this->onDeviceChanged(this->ui->deviceCombo->currentIndex());
  this->onFormatChanged(this->ui->formatCombo->currentIndex());
  this->onCheckButtonsToggled(false);
  this->onSpinsChanged();
  this->onBandwidthChanged(this->ui->bandwidthSpinBox->value());
  this->onAnalyzerTypeChanged(this->ui->analyzerTypeCombo->currentIndex());
}

void
ConfigDialog::connectAll(void)
{
  connect(
        this->ui->deviceCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onDeviceChanged(int)));

  connect(
        this->ui->antennaCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onAntennaChanged(int)));

  connect(
        this->ui->loadProfileButton,
        SIGNAL(clicked()),
        this,
        SLOT(onLoadProfileClicked(void)));

  connect(
        this->ui->sdrRadio,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onToggleSourceType(bool)));

  connect(
        this->ui->fileRadio,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onToggleSourceType(bool)));

  connect(
        this->ui->frequencySpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->lnbSpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->sampleRateSpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->decimationSpin,
        SIGNAL(valueChanged(int)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->sampleRateCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->ppmSpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onSpinsChanged(void)));

  connect(
        this->ui->removeDCCheck,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onCheckButtonsToggled(bool)));

  connect(
        this->ui->iqBalanceCheck,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onCheckButtonsToggled(bool)));

  connect(
        this->ui->loopCheck,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onCheckButtonsToggled(bool)));

  connect(
        this->ui->reverseDragBehaviorCheck,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onCheckButtonsToggled(bool)));


  connect(
        this->ui->bandwidthSpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onBandwidthChanged(double)));

  connect(
        this->ui->formatCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onFormatChanged(int)));

  connect(
        this,
        SIGNAL(accepted(void)),
        this,
        SLOT(onAccepted(void)));

  connect(
        this->ui->browseButton,
        SIGNAL(clicked(void)),
        this,
        SLOT(onBrowseCaptureFile(void)));

  connect(
        this->ui->saveProfileButton,
        SIGNAL(clicked(void)),
        this,
        SLOT(onSaveProfile(void)));

  connect(
        this->ui->analyzerTypeCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onAnalyzerTypeChanged(int)));

  connect(
        this->ui->hostEdit,
        SIGNAL(textEdited(const QString &)),
        this,
        SLOT(onRemoteParamsChanged()));

  connect(
        this->ui->portEdit,
        SIGNAL(valueChanged(int)),
        this,
        SLOT(onRemoteParamsChanged()));

  connect(
        this->ui->userEdit,
        SIGNAL(textEdited(const QString &)),
        this,
        SLOT(onRemoteParamsChanged()));

  connect(
        this->ui->passEdit,
        SIGNAL(textEdited(const QString &)),
        this,
        SLOT(onRemoteParamsChanged()));

  connect(
        this->ui->useNetworkProfileRadio,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onChangeConnectionType(void)));

  connect(
        this->ui->useHostPortRadio,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onChangeConnectionType(void)));

  connect(
        this->ui->remoteDeviceCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onRemoteProfileSelected(void)));

  connect(
        this->ui->refreshButton,
        SIGNAL(clicked(void)),
        this,
        SLOT(onRefreshRemoteDevices(void)));
}

void
ConfigDialog::setAnalyzerParams(const Suscan::AnalyzerParams &params)
{
  this->analyzerParams = params;
  this->refreshAnalyzerParamsUi();
}

void
ConfigDialog::setProfile(const Suscan::Source::Config &profile)
{
  this->profile = profile;
  this->refreshUi();
}

void
ConfigDialog::setFrequency(qint64 val)
{
  this->profile.setFreq(static_cast<SUFREQ>(val));
}

void
ConfigDialog::notifySingletonChanges(void)
{
  this->populateCombos();
  this->refreshUi();
}

bool
ConfigDialog::remoteSelected(void) const
{
  return this->ui->analyzerTypeCombo->currentIndex() == 1;
}

void
ConfigDialog::setGain(std::string const &name, float value)
{
  this->profile.setGain(name, value);
}

float
ConfigDialog::getGain(std::string const &name)
{
  return this->profile.getGain(name);
}

Suscan::AnalyzerParams
ConfigDialog::getAnalyzerParams(void)
{
  return this->analyzerParams;
}

Suscan::Source::Config
ConfigDialog::getProfile(void)
{
  return this->profile;
}

void
ConfigDialog::setColors(ColorConfig const &config)
{
  this->colors = config;
  this->refreshUi();
}

ColorConfig
ConfigDialog::getColors(void)
{
  return this->colors;
}

void
ConfigDialog::setGuiConfig(GuiConfig const &config)
{
  this->guiConfig = config;
  this->refreshUi();
}

GuiConfig
ConfigDialog::getGuiConfig()
{
  return this->guiConfig;
}

void
ConfigDialog::updateRemoteParams(void)
{
  this->profile.setParam("host", this->ui->hostEdit->text().toStdString());
  this->profile.setParam("port", std::to_string(this->ui->portEdit->value()));
  this->profile.setParam("user", this->ui->userEdit->text().toStdString());
  this->profile.setParam("password", this->ui->passEdit->text().toStdString());
}

ConfigDialog::ConfigDialog(QWidget *parent) :
  QDialog(parent),
  profile(SUSCAN_SOURCE_TYPE_FILE, SUSCAN_SOURCE_FORMAT_AUTO)
{
  this->ui = new Ui_Config();
  this->ui->setupUi(this);
  this->setWindowFlags(
    this->windowFlags() & ~Qt::WindowMaximizeButtonHint);
  this->layout()->setSizeConstraint(QLayout::SetFixedSize);

  // Setup remote device
  this->remoteDevice = Suscan::Source::Device(
          "Remote device",
          "localhost",
          28001,
          "anonymous",
          "");

  // Setup sample rate size
  this->ui->trueRateLabel->setFixedWidth(
        SuWidgetsHelpers::getWidgetTextWidth(
          this->ui->trueRateLabel,
          "XXX.XXX Xsps"));

  // Setup integer validators
  this->ui->fftSizeEdit->setValidator(new QIntValidator(1, 1 << 20, this));
  this->ui->spectrumRefreshEdit->setValidator(new QIntValidator(1, 1 << 20, this));
  this->ui->channelRefreshEdit->setValidator(new QIntValidator(1, 1 << 20, this));

  // Setup double validators
  this->ui->spectAvgAlphaEdit->setValidator(new QDoubleValidator(0., 1., 10, this));
  this->ui->sLevelAvgAlphaEdit->setValidator(new QDoubleValidator(0., 1., 10, this));
  this->ui->nLevelAvgAlphaEdit->setValidator(new QDoubleValidator(0., 1., 10, this));
  this->ui->snrThresholdEdit->setValidator(new QDoubleValidator(0., 10., 10, this));

  // Set limits
  this->ui->lnbSpinBox->setMaximum(300e9);
  this->ui->lnbSpinBox->setMinimum(-300e9);

  this->populateCombos();
  this->ui->sampleRateSpinBox->setUnits("sps");
  this->connectAll();
  this->refreshUi();
}

QString
ConfigDialog::getBaseName(const QString &path)
{
  int ndx;

  if ((ndx = path.lastIndexOf('/')) != -1)
    return path.right(path.size() - ndx - 1);

  return path;
}


ConfigDialog::~ConfigDialog()
{
  delete this->ui;
}

//////////////// Slots //////////////////
void
ConfigDialog::onLoadProfileClicked(void)
{
  QVariant data = this->ui->profileCombo->itemData(this->ui->profileCombo->currentIndex());

  this->profile = data.value<Suscan::Source::Config>();

  this->refreshUi();
}

void
ConfigDialog::onToggleSourceType(bool)
{
  if (!this->refreshing) {
    if (this->ui->sdrRadio->isChecked()) {
      this->profile.setType(SUSCAN_SOURCE_TYPE_SDR);
    } else {
      this->profile.setType(SUSCAN_SOURCE_TYPE_FILE);
      this->guessParamsFromFileName();
    }

    this->refreshUiState();
    this->refreshFrequencyLimits();
  }
}

void
ConfigDialog::onDeviceChanged(int index)
{
  // Remember: only set device if the analyzer type is local
  if (!this->refreshing
      && index != -1
      && !this->remoteSelected()) {
    Suscan::Singleton *sus = Suscan::Singleton::get_instance();
    const Suscan::Source::Device *device;

    SU_ATTEMPT(
          device = sus->getDeviceAt(
            static_cast<unsigned int>(
            this->ui->deviceCombo->itemData(index).value<long>())));

    this->profile.setDevice(*device);
    auto begin = device->getFirstAntenna();
    auto end   = device->getLastAntenna();

    // We check whether we can keep the current antenna configuration. If we
    // cannot, just set the first antenna in the list.
    if (device->findAntenna(this->profile.getAntenna()) == end
        && begin != end)
      this->profile.setAntenna(*begin);

    this->refreshUi();

    this->ui->bandwidthSpinBox->setValue(this->getSelectedSampleRate());
  }
}

void
ConfigDialog::onFormatChanged(int index)
{
  if (!this->refreshing) {
    switch (index) {
      case 0:
        this->profile.setFormat(SUSCAN_SOURCE_FORMAT_AUTO);
        break;

      case 1:
        this->profile.setFormat(SUSCAN_SOURCE_FORMAT_RAW_FLOAT32);
        break;

      case 2:
        this->profile.setFormat(SUSCAN_SOURCE_FORMAT_RAW_UNSIGNED8);
        break;

      case 3:
        this->profile.setFormat(SUSCAN_SOURCE_FORMAT_WAV);
        break;
    }
  }
}

void
ConfigDialog::onAntennaChanged(int)
{
  if (!this->refreshing)
    this->profile.setAntenna(
          this->ui->antennaCombo->currentText().toStdString());
}

void
ConfigDialog::onAnalyzerTypeChanged(int index)
{
  if (!this->refreshing) {
    switch (index) {
      case 0:
        this->profile.setInterface(SUSCAN_SOURCE_LOCAL_INTERFACE);
        this->onDeviceChanged(this->savedLocalDeviceIndex);
        break;

      case 1:
        this->savedLocalDeviceIndex = this->ui->deviceCombo->currentIndex();
        this->profile.setInterface(SUSCAN_SOURCE_REMOTE_INTERFACE);
        this->onChangeConnectionType();
        this->onRemoteParamsChanged();
        break;
    }

    this->refreshUiState();
  }
}

void
ConfigDialog::onRemoteParamsChanged(void)
{
  if (this->remoteSelected()) {
    this->profile.setDevice(this->remoteDevice);
    this->updateRemoteParams();
  }
}

void
ConfigDialog::onCheckButtonsToggled(bool)
{
  if (!this->refreshing) {
    this->profile.setDCRemove(this->ui->removeDCCheck->isChecked());
    this->profile.setIQBalance(this->ui->iqBalanceCheck->isChecked());
    this->profile.setLoop(this->ui->loopCheck->isChecked());
  }
}

unsigned int
ConfigDialog::getSelectedSampleRate(void) const
{
  unsigned int sampRate = 0;

  if (this->ui->sampRateStack->currentIndex() == 0) {
    // Index 0: Sample Rate Combo
    if (this->ui->sampleRateCombo->currentIndex() != -1) {
      qreal selectedValue =
          this->ui->sampleRateCombo->currentData().value<qreal>();
      sampRate = static_cast<unsigned>(selectedValue);
    }
  } else {
    // Index 1: Sample Rate Spin
    sampRate = static_cast<unsigned>(
          this->ui->sampleRateSpinBox->value());
  }

  return sampRate;
}

void
ConfigDialog::setSelectedSampleRate(unsigned int rate)
{
  // Set sample rate in both places
  qreal dist = std::numeric_limits<qreal>::infinity();
  int bestIndex = -1;
  for (auto i = 0; i < this->ui->sampleRateCombo->count(); ++i) {
    qreal value = this->ui->sampleRateCombo->itemData(i).value<qreal>();
    if (fabs(value - rate) < dist) {
      bestIndex = i;
      dist = fabs(value - rate);
    }
  }

  if (bestIndex != -1)
    this->ui->sampleRateCombo->setCurrentIndex(bestIndex);

  this->ui->sampleRateSpinBox->setValue(rate);
}

void
ConfigDialog::onSpinsChanged(void)
{
  if (!this->refreshing) {
    SUFREQ freq;
    SUFREQ lnbFreq;
    SUFLOAT ppm;
    unsigned int sampRate;

    lnbFreq = this->ui->lnbSpinBox->value();
    this->refreshFrequencyLimits();
    freq = this->ui->frequencySpinBox->value();
    sampRate = this->getSelectedSampleRate();
    ppm = static_cast<float>(this->ui->ppmSpinBox->value());

    this->profile.setFreq(freq);
    this->profile.setLnbFreq(lnbFreq);
    this->profile.setSampleRate(sampRate);
    this->profile.setDecimation(
          static_cast<unsigned>(this->ui->decimationSpin->value()));
    this->profile.setPPM(ppm);

    if (sender() == static_cast<QObject *>(this->ui->sampleRateCombo)
        || sender() == static_cast<QObject *>(this->ui->sampleRateSpinBox))
      this->ui->bandwidthSpinBox->setValue(
          sampRate / static_cast<qreal>(this->ui->decimationSpin->value()));

    this->refreshTrueSampleRate();
  }
}

bool
ConfigDialog::run(void)
{
  this->accepted = false;

  this->exec();

  return this->accepted;
}

void
ConfigDialog::onBandwidthChanged(double)
{
  if (!this->refreshing)
    this->profile.setBandwidth(
        static_cast<SUFLOAT>(
          this->ui->bandwidthSpinBox->value()));
}

void
ConfigDialog::onAccepted(void)
{
  this->saveGuiConfigUi();
  this->saveColors();
  this->saveAnalyzerParams();

  // warning: it will trigger reconfiguring device
  // and gui refresh from stored variables
  this->saveProfile();
  this->accepted = true;
}

void
ConfigDialog::guessParamsFromFileName(void)
{
  QFileInfo fi(QString::fromStdString(this->profile.getPath()));
  std::string baseName = fi.baseName().toStdString();
  SUFREQ fc;
  unsigned int fs;
  unsigned int date, time;
  bool haveFc = false;
  bool haveFs = false;

  if (sscanf(
        baseName.c_str(),
        "sigdigger_%08d_%06dZ_%d_%lg_float32_iq",
        &date,
        &time,
        &fs,
        &fc) == 4) {
    haveFc = true;
    haveFs = true;
  } else if (sscanf(
        baseName.c_str(),
        "sigdigger_%d_%lg_float32_iq",
        &fs,
        &fc) == 2) {
    haveFc = true;
    haveFs = true;
  } else if (sscanf(
        baseName.c_str(),
        "gqrx_%08d_%06d_%lg_%d_fc",
        &date,
        &time,
        &fc,
        &fs) == 4) {
    haveFc = true;
    haveFs = true;
  } else if (sscanf(
        baseName.c_str(),
        "SDRSharp_%08d_%06dZ_%lg_IQ",
        &date,
        &time,
        &fc) == 3) {
    haveFc = true;
  }

  if (haveFs)
    this->profile.setSampleRate(fs);

  if (haveFc)
    this->profile.setFreq(fc);

  if (haveFs || haveFc)
    this->refreshUi();
}

void
ConfigDialog::onBrowseCaptureFile(void)
{
  QString format;
  QString title;

  switch (this->profile.getFormat()) {
    case SUSCAN_SOURCE_FORMAT_AUTO:
      title = "Open capture file";
      format = "I/Q files (*.raw);;WAV files (*.wav);;All files (*)";
      break;

    case SUSCAN_SOURCE_FORMAT_RAW_FLOAT32:
      title = "Open I/Q file";
      format = "I/Q files (*.raw);;All files (*)";
      break;

    case SUSCAN_SOURCE_FORMAT_RAW_UNSIGNED8:
      title = "Open I/Q file";
      format = "I/Q files (*.raw);;All files (*)";
      break;

    case SUSCAN_SOURCE_FORMAT_WAV:
      title = "Open WAV file";
      format = "WAV files (*.wav);;All files (*)";
      break;
  }

  QString path = QFileDialog::getOpenFileName(
         this,
         title,
         QString(),
         format);


  if (!path.isEmpty()) {
    this->ui->pathEdit->setText(path);
    this->profile.setPath(path.toStdString());
    this->guessParamsFromFileName();
  }
}

void
ConfigDialog::onSaveProfile(void)
{
  Suscan::Singleton *sus = Suscan::Singleton::get_instance();
  std::string name = "My " + this->profile.label();
  std::string candidate = name;
  unsigned int i = 1;

  while (sus->getProfile(candidate) != nullptr)
    candidate = name + " (" + std::to_string(i++) + ")";

  this->saveProfileDialog.setProfileName(QString::fromStdString(candidate));

  if (this->saveProfileDialog.run()) {
    candidate = this->saveProfileDialog.getProfileName().toStdString();

    if (sus->getProfile(candidate) != nullptr) {
      QMessageBox::warning(
            this,
            "Profile already exists",
            "There is already a profile named " +
            this->saveProfileDialog.getProfileName() +
            " please choose a different one.",
            QMessageBox::Ok);
      return;
    }

    this->profile.setLabel(candidate);
    sus->saveProfile(this->profile);
    this->populateCombos();
  }
}

void
ConfigDialog::onChangeConnectionType(void)
{
  if (this->ui->useNetworkProfileRadio->isChecked()) {
    this->onRemoteProfileSelected();
    this->ui->useHostPortRadio->setChecked(false);
  }

  if (this->ui->useHostPortRadio->isChecked()) {
    this->onRemoteParamsChanged();
    this->ui->useNetworkProfileRadio->setChecked(false);
  }

  this->refreshUiState();
}

void
ConfigDialog::onRefreshRemoteDevices(void)
{
  Suscan::Singleton *sus = Suscan::Singleton::get_instance();
  int countBefore = this->ui->remoteDeviceCombo->count();
  int countAfter;

  sus->refreshNetworkProfiles();
  this->populateCombos();

  countAfter = this->ui->remoteDeviceCombo->count();

  if (countAfter > countBefore) {
    this->ui->useNetworkProfileRadio->setChecked(true);
    this->onChangeConnectionType();
  } else {
    this->refreshUiState();
  }
}

void
ConfigDialog::onRemoteProfileSelected(void)
{
  Suscan::Singleton *sus = Suscan::Singleton::get_instance();

  if (this->ui->useNetworkProfileRadio->isChecked()) {
    QHash<QString, Suscan::Source::Config>::const_iterator it;

    it = sus->getNetworkProfileFrom(this->ui->remoteDeviceCombo->currentText());

    if (it != sus->getLastNetworkProfile()) {
      this->setProfile(*it);

      // Provide a better hint for username if the server announced none
      if (this->profile.getParam("user").length() == 0)
        this->ui->userEdit->setText("anonymous");
      this->updateRemoteParams();
    }
  }
}
