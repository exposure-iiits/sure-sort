#include "settings_dialog.h"
#include "destination_dialog.h"
#include "app_version.h"
#include "output_dialog.h"
#include "shortcut_manager.h"
#include "sorting_controller.h"
#include "watermark_dialog.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QSlider>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>

namespace photocull {
namespace {
void embedDialog(QDialog* dialog) {
    dialog->setWindowFlags(Qt::Widget);
    dialog->setMinimumSize(0, 0);
    if (auto* layout = dialog->layout()) layout->setContentsMargins(0, 10, 0, 0);
    // The settings window owns the Save/Close actions.
    for (auto* box : dialog->findChildren<QDialogButtonBox*>()) {
        for (auto* button : box->buttons())
            button->setVisible(box->standardButton(button) == QDialogButtonBox::RestoreDefaults);
        if (!box->button(QDialogButtonBox::RestoreDefaults)) box->hide();
    }
}
}

SettingsDialog::SettingsDialog(SortingController* sorting, ShortcutManager* shortcuts, QWidget* parent)
    : QDialog(parent), sorting_(sorting), shortcuts_(shortcuts) {
    setObjectName("settingsDialog");
    setWindowTitle("Sure Sort · Settings");
    setWindowModality(Qt::WindowModal);
    resize(960, 640);
    setMinimumSize(800, 540);
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    sidebar_ = new QListWidget(this);
    sidebar_->setObjectName("settingsSidebar");
    sidebar_->setFixedWidth(160);
    sidebar_->addItems({"Sorting", "Watermark", "Editing", "Saving", "Viewing", "About"});
    outer->addWidget(sidebar_);
    auto* content = new QVBoxLayout;
    content->setContentsMargins(24, 22, 24, 18);
    content->setSpacing(10);
    title_ = new QLabel(this);
    title_->setObjectName("settingsTitle");
    description_ = new QLabel(this);
    description_->setObjectName("settingsDescription");
    description_->setWordWrap(true);
    content->addWidget(title_);
    content->addWidget(description_);
    pages_ = new QStackedWidget(this);
    pages_->setObjectName("settingsPages");
    content->addWidget(pages_, 1);

    destinations_ = new DestinationDialog(sorting_->config(), shortcuts_, this);
    embedDialog(destinations_);
    pages_->addWidget(destinations_);

    auto* watermark = new WatermarkDialog(sorting_->watermark(), this);
    embedDialog(watermark);
    watermark->layout()->setAlignment(Qt::AlignTop);
    pages_->addWidget(watermark);
    connect(watermark, &WatermarkDialog::previewInteractionChanged, this, [this](bool adjusting) {
        setWindowOpacity(adjusting && pages_->currentIndex() == Watermark ? 0.15 : 1.0);
    });
    connect(watermark, &WatermarkDialog::changed, this, [this, watermark](const WatermarkSnapshot& snapshot) {
        if (!sorting_->setWatermark(snapshot)) watermark->showError("Watermark could not be saved. Check the processing status.");
    });

    auto* editing = new QWidget(this);
    auto* editingLayout = new QVBoxLayout(editing);
    editingLayout->setContentsMargins(0, 12, 0, 0);
    auto* editingHelp = new QLabel(
        "Edits save per photo. Use sliders or customize keys in Keyboard shortcuts.", editing);
    editingHelp->setWordWrap(true);
    editingLayout->addWidget(editingHelp);
    auto* stepTitle = new QLabel("Keyboard editing step", editing);
    editingLayout->addWidget(stepTitle);
    auto* stepRow = new QHBoxLayout;
    auto* step = new QSlider(Qt::Horizontal, editing);
    step->setObjectName("editingStep");
    step->setAccessibleName("Keyboard editing step");
    step->setRange(1, 20);
    step->setValue(sorting_->editingStep());
    auto* stepValue = new QLabel(editing);
    stepValue->setObjectName("editingStepValue");
    auto updateStepLabel = [stepValue](int value) {
        stepValue->setText(QString("%1 units · %2 EV").arg(value).arg(value / 10.0, 0, 'f', 1));
    };
    updateStepLabel(step->value());
    stepRow->addWidget(step, 1);
    stepRow->addWidget(stepValue);
    editingLayout->addLayout(stepRow);
    auto* stepHelp = new QLabel("Change per key press. Rotation uses 90° steps.", editing);
    stepHelp->setWordWrap(true);
    editingLayout->addWidget(stepHelp);
    connect(step, &QSlider::valueChanged, this, [this, step, updateStepLabel](int value) {
        auto preferences = sorting_->uiPreferences();
        preferences["editingStep"] = value;
        if (sorting_->saveUiPreferences(preferences)) { updateStepLabel(value); feedback_->clear(); }
        else {
            const QSignalBlocker blocker(step);
            step->setValue(sorting_->editingStep());
            feedback_->setText("Editing step could not be saved.");
        }
    });
    auto* openEditing = new QPushButton("Open editing panel", editing);
    openEditing->setObjectName("openEditingPanel");
    editingLayout->addWidget(openEditing, 0, Qt::AlignLeft);
    editingLayout->addStretch();
    connect(openEditing, &QPushButton::clicked, this, [this] { close(); emit editingRequested(); });
    pages_->addWidget(editing);

    auto* output = new OutputDialog(sorting_->output(), this);
    embedDialog(output);
    output->layout()->setAlignment(Qt::AlignTop);
    pages_->addWidget(output);
    connect(output, &OutputDialog::changed, this, [this](OutputSettings settings) {
        if (!sorting_->setOutput(settings)) feedback_->setText("Output settings could not be saved. Check the processing status.");
    });

    auto* viewing=new QWidget(this);auto* viewingLayout=new QVBoxLayout(viewing);viewingLayout->setContentsMargins(0,16,0,0);viewingLayout->setSpacing(12);
    auto* filmstrip=new QCheckBox("Show photo thumbnail strip",viewing);filmstrip->setObjectName("showFilmstrip");filmstrip->setChecked(sorting_->uiPreferences().value("showFilmstrip").toBool(true));viewingLayout->addWidget(filmstrip);
    auto* destinationToast=new QCheckBox("Show destination folder toast after copying",viewing);destinationToast->setObjectName("showDestinationToast");destinationToast->setChecked(sorting_->uiPreferences().value("showDestinationToast").toBool(false));viewingLayout->addWidget(destinationToast);
    connect(destinationToast,&QCheckBox::toggled,this,[this,destinationToast](bool enabled){auto preferences=sorting_->uiPreferences();preferences["showDestinationToast"]=enabled;if(!sorting_->saveUiPreferences(preferences)){QSignalBlocker block(destinationToast);destinationToast->setChecked(!enabled);feedback_->setText("Destination toast preference could not be saved.");}});
    auto* positionLabel=new QLabel("Thumbnail strip position",viewing);viewingLayout->addWidget(positionLabel);
    auto* position=new QComboBox(viewing);position->setObjectName("filmstripPosition");for(const auto& item:QStringList{"Bottom","Top","Left","Right"})position->addItem(item,item.toLower());
    const int savedPosition=position->findData(sorting_->uiPreferences().value("filmstripPosition").toString("left"));position->setCurrentIndex(qMax(0,savedPosition));viewingLayout->addWidget(position,0,Qt::AlignLeft);
    connect(position,&QComboBox::currentIndexChanged,this,[this,position](int){auto preferences=sorting_->uiPreferences();const auto previous=preferences.value("filmstripPosition").toString("left");preferences["filmstripPosition"]=position->currentData().toString();if(sorting_->saveUiPreferences(preferences)){feedback_->clear();emit viewerPreferencesChanged();}else{QSignalBlocker block(position);position->setCurrentIndex(qMax(0,position->findData(previous)));feedback_->setText("Thumbnail position could not be saved.");}});
    auto* viewingHelp=new QLabel("Click a thumbnail to jump to that photo. Place the cursor over the strip and use the mouse wheel or trackpad to scroll.\n\nFolders and thumbnails stay visible in fullscreen; only the photo status row is hidden. Preferences save automatically.",viewing);viewingHelp->setWordWrap(true);viewingLayout->addWidget(viewingHelp);viewingLayout->addStretch();pages_->addWidget(viewing);
    connect(filmstrip,&QCheckBox::toggled,this,[this,filmstrip](bool enabled){auto preferences=sorting_->uiPreferences();preferences["showFilmstrip"]=enabled;if(sorting_->saveUiPreferences(preferences)){feedback_->clear();emit viewerPreferencesChanged();}else{QSignalBlocker block(filmstrip);filmstrip->setChecked(!enabled);feedback_->setText("Thumbnail preference could not be saved.");}});

    auto* about = new QLabel(
        "<span style='font-size:24px;font-weight:600'>Sure Sort</span><br><br>"
        "A quiet workspace for browsing, editing and sorting photographs.<br><br>"
        "Copy jobs run in the background. Processing status and errors remain visible below the photograph.<br><br>"
        "Your edits and sorting progress are saved automatically.<br><br>"
        "<b>Version " SURESORT_VERSION "</b><br><br>"
        "<b>Versioning</b><br>"
        "MAJOR.MINOR.PATCH<br>"
        "Major: incompatible changes. Minor: new features. Patch: fixes and refinements.<br>"
        "During 0.x development, minor releases may include breaking changes.<br><br>"
        "Qt " + QString::fromLatin1(qVersion()), this);
    about->setObjectName("aboutVersion");
    about->setWordWrap(true);
    about->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    about->setContentsMargins(0, 20, 0, 0);
    pages_->addWidget(about);

    feedback_ = new QLabel(this);
    feedback_->setObjectName("settingsFeedback");
    feedback_->setWordWrap(true);
    content->addWidget(feedback_);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    save_ = new QPushButton(this);
    save_->setObjectName("saveSettingsPage");
    auto* closeButton = new QPushButton("Close", this);
    closeButton->setObjectName("closeSettings");
    buttons->addWidget(save_);
    buttons->addWidget(closeButton);
    content->addLayout(buttons);
    outer->addLayout(content, 1);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(save_, &QPushButton::clicked, this, &SettingsDialog::saveCurrentPage);
    connect(sidebar_, &QListWidget::currentRowChanged, this, &SettingsDialog::selectPage);
    sidebar_->setCurrentRow(Sorting);
}

void SettingsDialog::showPage(Page page) {
    sidebar_->setCurrentRow(page);
    show();
    raise();
    activateWindow();
}

void SettingsDialog::selectPage(int page) {
    setWindowOpacity(1.0);
    const QStringList titles = {"Sorting folders", "Watermark", "Quick editing", "Saving", "Viewing", "About"};
    const QStringList descriptions = {
        "Choose an output folder, then give each sorting folder a key. Save folders to apply changes.",
        "Saved across folders. Adjust a slider to see through this window and preview the photo.",
        "Edit with the keyboard, or reveal the sliders beside your photograph.",
        "Quality, image size and metadata. Changes save automatically for future assignments.",
        "Choose how you browse your photographs.",
        "Fast, local photo culling."
    };
    if (page < 0 || page >= titles.size()) return;
    pages_->setCurrentIndex(page);
    title_->setText(titles[page]);
    description_->setText(descriptions[page]);
    feedback_->clear();
    save_->setVisible(page == Sorting);
    save_->setEnabled(sorting_->ready());
    save_->setText("Save folders");
}

void SettingsDialog::hideEvent(QHideEvent* event) {
    setWindowOpacity(1.0);
    QDialog::hideEvent(event);
}

void SettingsDialog::saveCurrentPage() {
    QString error;
    if (sidebar_->currentRow() == Sorting) {
        const auto config = destinations_->configuration();
        error = validateDestinationConfig(config);
        QVector<QPair<QString, QKeySequence>> bindings;
        for (const auto& folder : config.folders) {
            bindings.append({folder.id, QKeySequence(folder.shortcut, QKeySequence::PortableText)});
            bindings.append({folder.id, QKeySequence(folder.alternateShortcut, QKeySequence::PortableText)});
        }
        if (error.isEmpty()) shortcuts_->validateDestinations(bindings, error);
        if (error.isEmpty() && !sorting_->configure(config)) error = "Sorting folders could not be saved. Check the processing status.";
    }
    feedback_->setText(error.isEmpty() ? "Saved." : error);
}
}
