#include "WhatCanIBuildWidget.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ThemeCatalogRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../services/application/RemoteReadApplicationServices.h"
#include "../../services/application/RemoteCollectionMutationApplicationService.h"
#include "../../services/builds/PartUsageDiscoveryService.h"
#include "../../services/builds/InventoryBuildabilityService.h"
#include "../../services/images/SetImageService.h"
#include "../../services/parts/PartResolver.h"
#include "../../settings/UserSettings.h"
#include "../catalog/SetDetailsDialog.h"
#include "../helpers/PartSearchCompleterHelper.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

WhatCanIBuildWidget::WhatCanIBuildWidget(WorkspaceContext& workspaceContext, QWidget* parent,
    RemoteReadApplicationServices* remoteReads,
    RemoteCollectionMutationApplicationService* remoteCollection)
    : QWidget(parent), m_workspaceContext(workspaceContext), m_remoteReads(remoteReads),
      m_remoteCollection(remoteCollection)
{
    m_service = new PartUsageDiscoveryService(DatabaseManager::instance().databasePath(), this);
    m_inventoryService = new InventoryBuildabilityService(DatabaseManager::instance().databasePath(), this);
    m_images = new SetImageService(this);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(QStringLiteral("Discover Sets using loose Inventory and eligible Collection items, or find Sets that use selected Parts. Results are advisory and do not reserve Inventory."), this);
    intro->setWordWrap(true); layout->addWidget(intro);

    auto* modeRow=new QHBoxLayout;
    m_modeCombo=new QComboBox(this);m_modeCombo->addItem(QStringLiteral("From My Inventory"));m_modeCombo->addItem(QStringLiteral("Using Selected Parts"));
    modeRow->addWidget(new QLabel(QStringLiteral("Mode:"),this));modeRow->addWidget(m_modeCombo);modeRow->addStretch();layout->addLayout(modeRow);
    m_remoteInventoryNotice=new QLabel(QStringLiteral("Inventory buildability is currently available on the Host. Selected Parts discovery remains available on this Remote."),this);m_remoteInventoryNotice->setWordWrap(true);m_remoteInventoryNotice->setVisible(m_remoteReads!=nullptr);layout->addWidget(m_remoteInventoryNotice);
    m_modeStack=new QStackedWidget(this);layout->addWidget(m_modeStack);

    auto* inventoryPage=new QWidget(this);auto* inventoryLayout=new QVBoxLayout(inventoryPage);inventoryLayout->setContentsMargins(0,0,0,0);
    auto* inventoryFilters=new QHBoxLayout;
    m_minimumSpin=new QSpinBox(this);m_minimumSpin->setRange(0,100);m_minimumSpin->setSuffix(QStringLiteral("%"));m_minimumSpin->setValue(UserSettings::instance().whatCanIBuildMinimumBuildability());
    m_minimumSetPartsSpin=new QSpinBox(this);m_minimumSetPartsSpin->setRange(1,10000);m_minimumSetPartsSpin->setValue(UserSettings::instance().whatCanIBuildMinimumSetParts());
    m_yearFromSpin=new QSpinBox(this);m_yearFromSpin->setRange(0,3000);m_yearFromSpin->setSpecialValueText(QStringLiteral("Any"));
    m_yearToSpin=new QSpinBox(this);m_yearToSpin->setRange(0,3000);m_yearToSpin->setSpecialValueText(QStringLiteral("Any"));
    m_themeCombo=new QComboBox(this);loadThemes();
    m_fullyOnly=new QCheckBox(QStringLiteral("Fully buildable only"),this);
    m_includeCollection=new QCheckBox(QStringLiteral("Include eligible Collection Sets"),this);m_includeCollection->setChecked(true);
    m_fullyFirst=new QCheckBox(QStringLiteral("Fully buildable first"),this);m_fullyFirst->setChecked(UserSettings::instance().whatCanIBuildFullyBuildableFirst());
    inventoryFilters->addWidget(new QLabel(QStringLiteral("Minimum:"),this));inventoryFilters->addWidget(m_minimumSpin);inventoryFilters->addWidget(new QLabel(QStringLiteral("Minimum Set Parts:"),this));inventoryFilters->addWidget(m_minimumSetPartsSpin);inventoryFilters->addWidget(new QLabel(QStringLiteral("Year From:"),this));inventoryFilters->addWidget(m_yearFromSpin);inventoryFilters->addWidget(new QLabel(QStringLiteral("To:"),this));inventoryFilters->addWidget(m_yearToSpin);inventoryFilters->addWidget(new QLabel(QStringLiteral("Theme:"),this));inventoryFilters->addWidget(m_themeCombo,1);inventoryLayout->addLayout(inventoryFilters);
    auto* inventoryOptions=new QHBoxLayout;inventoryOptions->addWidget(m_fullyOnly);inventoryOptions->addWidget(m_includeCollection);inventoryOptions->addWidget(m_fullyFirst);inventoryOptions->addStretch();inventoryLayout->addLayout(inventoryOptions);
    auto* inventorySearchRow=new QHBoxLayout;m_inventorySearchEdit=new QLineEdit(this);m_inventorySearchEdit->setPlaceholderText(QStringLiteral("Optional Set number or name"));m_inventorySearchButton=new QPushButton(QStringLiteral("Evaluate Buildability"),this);inventorySearchRow->addWidget(new QLabel(QStringLiteral("Search:"),this));inventorySearchRow->addWidget(m_inventorySearchEdit,1);inventorySearchRow->addWidget(m_inventorySearchButton);inventoryLayout->addLayout(inventorySearchRow);
    m_collectionStatus=new QLabel(QStringLiteral("Collection sources are evaluated from active, complete, opted-in Set items."),this);m_collectionStatus->setToolTip(QStringLiteral("Collection Sets must be active, Complete, opted in, and have local composition to contribute advisory pieces."));inventoryLayout->addWidget(m_collectionStatus);
    m_modeStack->addWidget(inventoryPage);

    auto* selectedPage=new QWidget(this);auto* selectedLayout=new QVBoxLayout(selectedPage);selectedLayout->setContentsMargins(0,0,0,0);

    auto* add = new QHBoxLayout;
    m_partEdit = new QLineEdit(this); m_partEdit->setPlaceholderText(QStringLiteral("Part number"));
    PartSearchCompleterHelper::install(m_partEdit, [this] { updateResolvedPartSelection(); });
    m_colorCombo = new QComboBox(this); loadColors();
    m_quantitySpin = new QSpinBox(this); m_quantitySpin->setRange(1, 999999); m_quantitySpin->setValue(1);
    m_addButton = new QPushButton(QStringLiteral("Add Selected Part"), this);
    add->addWidget(new QLabel(QStringLiteral("Part:"), this)); add->addWidget(m_partEdit, 2);
    add->addWidget(new QLabel(QStringLiteral("Color:"), this)); add->addWidget(m_colorCombo, 1);
    add->addWidget(new QLabel(QStringLiteral("Qty:"), this)); add->addWidget(m_quantitySpin); add->addWidget(m_addButton);
    selectedLayout->addLayout(add);

    m_criteriaTable = new QTableWidget(this); m_criteriaTable->setColumnCount(5);
    m_criteriaTable->setHorizontalHeaderLabels({QStringLiteral("Part #"),QStringLiteral("Description"),QStringLiteral("Color"),QStringLiteral("Qty"),QStringLiteral("Action")});
    m_criteriaTable->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    m_criteriaTable->verticalHeader()->setVisible(false); m_criteriaTable->setMaximumHeight(180);
    m_criteriaTable->setEditTriggers(QAbstractItemView::NoEditTriggers); selectedLayout->addWidget(m_criteriaTable);

    auto* filters = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this); m_searchEdit->setPlaceholderText(QStringLiteral("Set number or name"));
    m_matchCombo = new QComboBox(this); m_matchCombo->addItem(QStringLiteral("All Selected Parts"),int(PartUsageMatchMode::All)); m_matchCombo->addItem(QStringLiteral("Any Selected Part"),int(PartUsageMatchMode::Any));
    if (!UserSettings::instance().whatCanIBuildRequireAllParts()) m_matchCombo->setCurrentIndex(1);
    m_pageSizeCombo = new QComboBox(this); for(int n:{100,250,500})m_pageSizeCombo->addItem(QString::number(n),n);
    m_pageSizeCombo->setCurrentIndex(m_pageSizeCombo->findData(UserSettings::instance().whatCanIBuildRowsPerPage()));
    m_searchButton = new QPushButton(QStringLiteral("Find Matching Sets"),this);
    filters->addWidget(new QLabel(QStringLiteral("Search:"),this));filters->addWidget(m_searchEdit,2);
    filters->addWidget(new QLabel(QStringLiteral("Match:"),this));filters->addWidget(m_matchCombo);
    filters->addWidget(new QLabel(QStringLiteral("Rows:"),this));filters->addWidget(m_pageSizeCombo);filters->addWidget(m_searchButton);
    selectedLayout->addLayout(filters);m_modeStack->addWidget(selectedPage);

    m_status = new QLabel(QStringLiteral("Select at least one Part to begin."),this);layout->addWidget(m_status);
    m_resultsTable = new QTableWidget(this);m_resultsTable->setColumnCount(8);
    m_resultsTable->setHorizontalHeaderLabels({QStringLiteral("Image"),QStringLiteral("Set #"),QStringLiteral("Name"),QStringLiteral("Year"),QStringLiteral("Theme"),QStringLiteral("Parts"),QStringLiteral("Selected Parts Match"),QStringLiteral("Actions")});
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    for(int c:{0,1,3,4,5,6,7})m_resultsTable->horizontalHeader()->setSectionResizeMode(c,QHeaderView::ResizeToContents);
    m_resultsTable->verticalHeader()->setVisible(false);m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_resultsTable,1);
    auto* paging=new QHBoxLayout;m_previous=new QPushButton(QStringLiteral("Previous"),this);m_next=new QPushButton(QStringLiteral("Next"),this);m_pageLabel=new QLabel(QStringLiteral("Page 1 of 1"),this);
    paging->addStretch();paging->addWidget(m_previous);paging->addWidget(m_pageLabel);paging->addWidget(m_next);layout->addLayout(paging);

    connect(m_addButton,&QPushButton::clicked,this,&WhatCanIBuildWidget::addCriterion);
    connect(m_partEdit,&QLineEdit::returnPressed,this,&WhatCanIBuildWidget::addCriterion);
    connect(m_searchButton,&QPushButton::clicked,this,&WhatCanIBuildWidget::search);
    connect(m_searchEdit,&QLineEdit::returnPressed,this,&WhatCanIBuildWidget::search);
    connect(m_searchEdit,&QLineEdit::textChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_matchCombo,&QComboBox::currentIndexChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_pageSizeCombo,&QComboBox::currentIndexChanged,this,[this]{m_page=0;showPage();});
    connect(m_previous,&QPushButton::clicked,this,[this]{if(m_page>0){--m_page;showPage();}});
    connect(m_next,&QPushButton::clicked,this,[this]{++m_page;showPage();});
    connect(m_modeCombo,&QComboBox::currentIndexChanged,this,&WhatCanIBuildWidget::modeChanged);
    connect(m_inventorySearchButton,&QPushButton::clicked,this,&WhatCanIBuildWidget::searchInventory);
    connect(m_inventorySearchEdit,&QLineEdit::returnPressed,this,&WhatCanIBuildWidget::searchInventory);
    connect(m_inventorySearchEdit,&QLineEdit::textChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_minimumSpin,&QSpinBox::valueChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_minimumSetPartsSpin,&QSpinBox::valueChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_yearFromSpin,&QSpinBox::valueChanged,this,[this]{markStale();updateControls();});
    connect(m_yearToSpin,&QSpinBox::valueChanged,this,[this]{markStale();updateControls();});
    connect(m_themeCombo,&QComboBox::currentIndexChanged,this,&WhatCanIBuildWidget::markStale);
    connect(m_fullyOnly,&QCheckBox::toggled,this,&WhatCanIBuildWidget::markStale);
    connect(m_includeCollection,&QCheckBox::toggled,this,&WhatCanIBuildWidget::markStale);
    connect(m_fullyFirst,&QCheckBox::toggled,this,&WhatCanIBuildWidget::markStale);
    connect(m_resultsTable,&QTableWidget::cellDoubleClicked,this,[this](int row,int){auto*i=m_resultsTable->item(row,1);if(i&&i->data(Qt::UserRole).toInt()>0)showDetails(i->data(Qt::UserRole).toInt());});
    connect(m_images,&SetImageService::imageReady,this,[this](const QString& number,const QString& path){for(int r=0;r<m_resultsTable->rowCount();++r)if(m_resultsTable->item(r,1)&&m_resultsTable->item(r,1)->text()==number){QPixmap p(path);if(!p.isNull())m_resultsTable->item(r,0)->setData(Qt::DecorationRole,p.scaled(48,48,Qt::KeepAspectRatio,Qt::SmoothTransformation));}});
    if(m_remoteReads){m_modeCombo->setCurrentIndex(1);updateRemoteCapability();}
    modeChanged();updateControls();
}

void WhatCanIBuildWidget::loadColors(){m_colorCombo->addItem(QStringLiteral("Any Color"),0);for(const auto&c:ColorRepository().getAll())m_colorCombo->addItem(c.name(),c.id());}
void WhatCanIBuildWidget::loadThemes(){m_themeCombo->addItem(QStringLiteral("All Themes"),0);m_themeCombo->setItemData(0,0,Qt::UserRole+1);for(const auto&t:ThemeCatalogRepository().activeSetFilterHierarchy()){m_themeCombo->addItem(t.qualifiedName,t.id);m_themeCombo->setItemData(m_themeCombo->count()-1,t.externalId.toInt(),Qt::UserRole+1);}}

void WhatCanIBuildWidget::updateResolvedPartSelection()
{
    m_resolvedPart.reset();
    const QString canonicalNumber = PartSearchCompleterHelper::canonicalPartNumber(m_partEdit);
    if (!canonicalNumber.isEmpty()) {
        const auto resolved = PartResolver().resolve(canonicalNumber);
        if (resolved.hasResolvedPart)
            m_resolvedPart = resolved.part;
    }
    updateControls();
}

void WhatCanIBuildWidget::addCriterion()
{
    updateResolvedPartSelection();
    if(!m_resolvedPart){QMessageBox::warning(this,QStringLiteral("Select Part"),QStringLiteral("Select or enter a Part that resolves uniquely in the BrickSuite catalog."));return;}
    const Part selectedPart=*m_resolvedPart;
    const int color=m_colorCombo->currentData().toInt(),qty=m_quantitySpin->value();
    auto found=std::find_if(m_criteria.begin(),m_criteria.end(),[&](const auto&x){return x.partId==selectedPart.id()&&x.colorId==color;});
    if(found!=m_criteria.end())found->quantity+=qty;
    else {if(m_criteria.size()>=20){QMessageBox::information(this,QStringLiteral("Selected Parts"),QStringLiteral("A search can contain at most 20 selected Part criteria."));return;}m_criteria.append({selectedPart.id(),selectedPart.partNumber(),selectedPart.name(),color,m_colorCombo->currentText(),qty});}
    renderCriteria();
    m_partEdit->clear();m_quantitySpin->setValue(1);markStale();updateControls();
}

void WhatCanIBuildWidget::renderCriteria()
{
    m_criteriaTable->setRowCount(0);
    for (const auto& criterion : std::as_const(m_criteria)) {
        const int row = m_criteriaTable->rowCount();
        m_criteriaTable->insertRow(row);
        m_criteriaTable->setItem(row, 0, new QTableWidgetItem(criterion.partNumber));
        m_criteriaTable->setItem(row, 1, new QTableWidgetItem(criterion.partName));
        m_criteriaTable->setItem(row, 2, new QTableWidgetItem(
            criterion.colorId > 0 ? criterion.colorName : QStringLiteral("Any Color")));
        m_criteriaTable->setItem(row, 3, new QTableWidgetItem(QString::number(criterion.quantity)));
        auto* removeButton = new QPushButton(QStringLiteral("Remove"), m_criteriaTable);
        connect(removeButton, &QPushButton::clicked, this, [this, removeButton] {
            const int currentRow = m_criteriaTable->indexAt(removeButton->pos()).row();
            if (currentRow < 0 || currentRow >= m_criteria.size()) return;
            m_criteria.removeAt(currentRow);
            renderCriteria();
            markStale();
            updateControls();
        });
        m_criteriaTable->setCellWidget(row, 4, removeButton);
    }
}

void WhatCanIBuildWidget::markStale(){const bool cancelled=m_searchOutstanding;if(cancelled){m_service->cancel();m_inventoryService->cancel();++m_generation;m_searchOutstanding=false;}if(m_hasSearched||cancelled){m_stale=true;m_status->setText(QStringLiteral("Search criteria changed — run discovery again."));emit statusMessageRequested(QStringLiteral("What Can I Build criteria changed."),5000);}updateControls();}
void WhatCanIBuildWidget::invalidateCatalog(){m_service->cancel();m_inventoryService->cancel();m_searchOutstanding=false;m_results.clear();m_inventoryResults.clear();m_page=0;m_stale=true;m_hasSearched=false;showPage();m_status->setText(QStringLiteral("Local catalog data changed — search again."));}
void WhatCanIBuildWidget::invalidateOperationalData(){if(!m_inventoryMode)return;m_inventoryService->cancel();++m_generation;++m_remoteDetailsGeneration;m_searchOutstanding=false;m_inventoryResults.clear();m_includeCollection->setEnabled(true);m_page=0;m_stale=true;m_hasSearched=false;showPage();m_status->setText(QStringLiteral("Results are out of date. Run Evaluate Buildability again."));updateControls();}

void WhatCanIBuildWidget::modeChanged(){m_inventoryMode=m_modeCombo->currentIndex()==0;m_modeStack->setCurrentIndex(m_inventoryMode?0:1);++m_generation;++m_remoteDetailsGeneration;m_results.clear();m_inventoryResults.clear();m_page=0;m_hasSearched=false;m_stale=false;showPage();m_status->setText(m_inventoryMode?QStringLiteral("Choose a minimum buildability and start discovery."):QStringLiteral("Select at least one Part to begin."));updateControls();}

void WhatCanIBuildWidget::setRemoteSessionConnected(bool connected){m_remoteSessionConnected=connected;++m_generation;++m_remoteDetailsGeneration;if(!connected&&m_inventoryMode){m_searchOutstanding=false;m_stale=true;if(m_hasSearched)m_status->setText(QStringLiteral("Host connection lost. Existing buildability results are out of date."));}updateRemoteCapability();updateControls();}

void WhatCanIBuildWidget::updateRemoteCapability(){if(!m_remoteReads)return;const bool capable=m_remoteReads->isAvailableFor(QStringLiteral("buildability.inventory.search"));m_modeCombo->setItemData(0,capable?QVariant():QVariant(0),Qt::UserRole-1);const QString explanation=capable?QString():QStringLiteral("Inventory buildability is not supported by the connected Host. Selected Parts discovery remains available.");m_modeCombo->setItemData(0,explanation,Qt::ToolTipRole);m_remoteInventoryNotice->setText(explanation);m_remoteInventoryNotice->setVisible(!capable);if(!capable&&m_modeCombo->currentIndex()==0)m_modeCombo->setCurrentIndex(1);}

void WhatCanIBuildWidget::search()
{
    if(m_inventoryMode){searchInventory();return;}
    if(m_criteria.isEmpty()){m_status->setText(QStringLiteral("Select at least one Part to begin."));return;}
    PartUsageSearch request;request.criteria=m_criteria;request.text=m_searchEdit->text();request.matchMode=PartUsageMatchMode(m_matchCombo->currentData().toInt());request.maximumResults=UserSettings::instance().whatCanIBuildMaximumResults();
    m_status->setText(QStringLiteral("Finding matching Sets..."));emit statusMessageRequested(m_status->text(),0);m_searchOutstanding=true;m_searchButton->setEnabled(false);m_page=0;m_stale=false;
    m_generation=m_service->search(request,this,[this](quint64 generation,const PartUsageSearchResult&result){if(generation!=m_generation)return;m_searchOutstanding=false;m_hasSearched=result.success;m_searchButton->setEnabled(true);if(!result.success){m_results.clear();showPage();const QString message=QStringLiteral("Unable to search the local catalog: %1").arg(result.errorMessage);m_status->setText(message);emit statusMessageRequested(message,5000);return;}m_results=result.sets;m_capReached=result.capReached;showPage();QString message;if(m_results.isEmpty())message=QStringLiteral("No matching Sets found.");else if(m_capReached)message=QStringLiteral("%1+ matching Sets found — result limit reached.").arg(m_results.size());else if(result.qualifyingCount==1)message=QStringLiteral("1 matching Set found.");else message=QStringLiteral("%1 matching Sets found.").arg(result.qualifyingCount);m_status->setText(message);emit statusMessageRequested(message,5000);});
}

void WhatCanIBuildWidget::searchInventory()
{
    if(m_yearFromSpin->value()>0&&m_yearToSpin->value()>0&&m_yearFromSpin->value()>m_yearToSpin->value()){m_status->setText(QStringLiteral("Year From cannot be later than Year To."));updateControls();return;}
    if(m_remoteReads){
        if(!m_remoteReads->isAvailableFor(QStringLiteral("buildability.inventory.search"))){updateRemoteCapability();m_status->setText(m_remoteSessionConnected?QStringLiteral("Inventory buildability is not supported by the connected Host."):QStringLiteral("Connect to BrickSuite Host to evaluate Inventory buildability."));updateControls();return;}
        RemoteBuildabilityDto::SearchRequest request;request.workspaceId=m_workspaceContext.currentWorkspaceId();request.text=m_inventorySearchEdit->text();request.minimumPercent=m_minimumSpin->value();request.minimumSetParts=m_minimumSetPartsSpin->value();request.yearFrom=m_yearFromSpin->value();request.yearTo=m_yearToSpin->value();request.rebrickableThemeId=m_themeCombo->currentData(Qt::UserRole+1).toInt();request.fullyBuildableOnly=m_fullyOnly->isChecked();request.includeCollection=m_includeCollection->isChecked();request.fullyBuildableFirst=m_fullyFirst->isChecked();request.maximumResults=qMin(UserSettings::instance().whatCanIBuildMaximumResults(),RemoteBuildabilityDto::MaximumResults);
        UserSettings::instance().setWhatCanIBuildMinimumBuildability(request.minimumPercent);UserSettings::instance().setWhatCanIBuildFullyBuildableFirst(request.fullyBuildableFirst);
        const quint64 generation=++m_generation;const int workspaceId=request.workspaceId;m_status->setText(QStringLiteral("Requesting Host buildability..."));emit statusMessageRequested(m_status->text(),0);m_searchOutstanding=true;m_inventorySearchButton->setEnabled(false);m_page=0;m_stale=false;
        m_remoteSearchToken=m_remoteReads->searchBuildability(request,this,[this,generation,workspaceId,includeCollection=request.includeCollection](AsyncReadResult<RemoteBuildabilityDto::SearchResponse> outcome){if(generation!=m_generation||workspaceId!=m_workspaceContext.currentWorkspaceId())return;m_searchOutstanding=false;if(!outcome.succeeded()){m_inventoryResults.clear();m_hasSearched=false;showPage();const QString message=QStringLiteral("Unable to evaluate What Can I Build: %1").arg(outcome.message);m_status->setText(message);emit statusMessageRequested(message,5000);updateControls();return;}const auto&response=*outcome.value;m_inventoryResults.clear();for(const auto&x:response.rows){InventoryBuildabilitySetResult row;row.setNumber=x.setNumber;row.name=x.name;row.year=x.year;row.rebrickableThemeId=x.rebrickableThemeId;row.themeName=x.qualifiedThemeName;row.imageUrl=x.imageUrl;row.catalogPartCount=x.catalogPartCount;row.totalQuantity=x.totalRequiredPieces;row.totalRequirements=x.totalRequirements;row.looseSatisfiedQuantity=x.looseSatisfiedPieces;row.looseSatisfiedRequirements=x.looseSatisfiedRequirements;row.advisorySatisfiedQuantity=x.advisorySatisfiedPieces;row.advisorySatisfiedRequirements=x.advisorySatisfiedRequirements;row.missingQuantity=x.missingPieces;row.collectionSourceCount=x.collectionSourceCount;if(const auto local=SetCatalogRepository().getBySetNumber(x.setNumber))row.setCatalogId=local->id();m_inventoryResults.append(row);}m_hasSearched=true;m_capReached=response.capReached;if(includeCollection){QString status=QStringLiteral("%1 eligible Collection Set%2 may contribute pieces").arg(response.eligibleCollectionSourceCount).arg(response.eligibleCollectionSourceCount==1?QString():QStringLiteral("s"));if(response.dormantCollectionSourceCount>0)status+=QStringLiteral("; %1 opted-in Set%2 %3 currently ineligible").arg(response.dormantCollectionSourceCount).arg(response.dormantCollectionSourceCount==1?QString():QStringLiteral("s")).arg(response.dormantCollectionSourceCount==1?QStringLiteral("is"):QStringLiteral("are"));m_collectionStatus->setText(status+QLatin1Char('.'));}else m_collectionStatus->setText(QStringLiteral("Collection sources are excluded from this result."));showPage();QString message;if(m_inventoryResults.isEmpty())message=QStringLiteral("No qualifying Sets found.");else if(response.capReached)message=QStringLiteral("%1 qualifying Sets found; showing the top %2.").arg(response.qualifyingCount).arg(response.returnedCount);else message=QStringLiteral("%1 qualifying Sets found.").arg(response.qualifyingCount);m_status->setText(message);emit statusMessageRequested(message,5000);updateControls();});
        return;
    }
    InventoryBuildabilitySearch request;request.workspaceId=m_workspaceContext.currentWorkspaceId();request.text=m_inventorySearchEdit->text();request.minimumPercent=m_minimumSpin->value();request.minimumSetParts=m_minimumSetPartsSpin->value();request.yearFrom=m_yearFromSpin->value();request.yearTo=m_yearToSpin->value();request.themeCatalogId=m_themeCombo->currentData().toInt();request.fullyBuildableOnly=m_fullyOnly->isChecked();request.includeCollection=m_includeCollection->isChecked();request.fullyBuildableFirst=m_fullyFirst->isChecked();request.maximumResults=UserSettings::instance().whatCanIBuildMaximumResults();
    UserSettings::instance().setWhatCanIBuildMinimumBuildability(request.minimumPercent);UserSettings::instance().setWhatCanIBuildFullyBuildableFirst(request.fullyBuildableFirst);
    m_status->setText(QStringLiteral("Evaluating local Inventory and Collection sources..."));emit statusMessageRequested(m_status->text(),0);m_searchOutstanding=true;m_inventorySearchButton->setEnabled(false);m_page=0;m_stale=false;
    m_generation=m_inventoryService->search(request,this,[this,includeCollection=request.includeCollection](quint64 generation,const InventoryBuildabilitySearchResult&result){if(generation!=m_generation)return;m_searchOutstanding=false;m_hasSearched=result.success;if(!result.success){m_inventoryResults.clear();showPage();m_status->setText(QStringLiteral("Unable to evaluate buildability: %1").arg(result.errorMessage));updateControls();return;}m_inventoryResults=result.sets;m_capReached=result.capReached;if(includeCollection){QString status=QStringLiteral("%1 eligible Collection Set%2 may contribute pieces").arg(result.eligibleCollectionSources).arg(result.eligibleCollectionSources==1?QString():QStringLiteral("s"));if(result.dormantCollectionSources>0)status+=QStringLiteral("; %1 additional opted-in Set%2 %3 currently ineligible").arg(result.dormantCollectionSources).arg(result.dormantCollectionSources==1?QString():QStringLiteral("s")).arg(result.dormantCollectionSources==1?QStringLiteral("is"):QStringLiteral("are"));m_collectionStatus->setText(status+QLatin1Char('.'));m_includeCollection->setEnabled(result.eligibleCollectionSources>0);}else{m_collectionStatus->setText(QStringLiteral("Collection sources are excluded from this result."));m_includeCollection->setEnabled(true);}showPage();QString message;if(m_inventoryResults.isEmpty())message=QStringLiteral("No Sets meet the selected buildability threshold.");else if(result.capReached)message=QStringLiteral("%1 qualifying Sets found; showing the top %2.").arg(result.qualifyingCount).arg(m_inventoryResults.size());else message=QStringLiteral("%1 qualifying Sets found.").arg(result.qualifyingCount);m_status->setText(message);emit statusMessageRequested(message,5000);updateControls();});
}

void WhatCanIBuildWidget::showPage()
{
    const int size=m_pageSizeCombo?m_pageSizeCombo->currentData().toInt():100;const int count=m_inventoryMode?m_inventoryResults.size():m_results.size();const int pages=qMax(1,(count+size-1)/size);if(m_page>=pages)m_page=pages-1;
    m_resultsTable->setRowCount(0);const int begin=m_page*size,end=qMin(begin+size,count);
    if(m_inventoryMode){m_resultsTable->setColumnCount(11);m_resultsTable->setHorizontalHeaderLabels({QStringLiteral("Image"),QStringLiteral("Set #"),QStringLiteral("Name"),QStringLiteral("Year"),QStringLiteral("Theme"),QStringLiteral("Parts"),QStringLiteral("Loose"),QStringLiteral("With Collection"),QStringLiteral("Missing"),QStringLiteral("Sources"),QStringLiteral("Actions")});m_resultsTable->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);for(int c:{0,1,3,4,5,6,7,8,9,10})m_resultsTable->horizontalHeader()->setSectionResizeMode(c,QHeaderView::ResizeToContents);
        for(int i=begin;i<end;++i){const auto&x=m_inventoryResults.at(i);int r=m_resultsTable->rowCount();m_resultsTable->insertRow(r);auto*image=new QTableWidgetItem;m_resultsTable->setItem(r,0,image);auto*num=new QTableWidgetItem(x.setNumber);num->setData(Qt::UserRole,x.setCatalogId);m_resultsTable->setItem(r,1,num);m_resultsTable->setItem(r,2,new QTableWidgetItem(x.name));m_resultsTable->setItem(r,3,new QTableWidgetItem(QString::number(x.year)));m_resultsTable->setItem(r,4,new QTableWidgetItem(x.themeName));m_resultsTable->setItem(r,5,new QTableWidgetItem(QStringLiteral("%1 requirements / %2 pieces").arg(x.totalRequirements).arg(x.totalQuantity)));m_resultsTable->setItem(r,6,new QTableWidgetItem(QStringLiteral("%1% pieces; %2/%3 requirements").arg(x.loosePercent()).arg(x.looseSatisfiedRequirements).arg(x.totalRequirements)));m_resultsTable->setItem(r,7,new QTableWidgetItem(QStringLiteral("%1% pieces; %2/%3 requirements").arg(x.advisoryPercent()).arg(x.advisorySatisfiedRequirements).arg(x.totalRequirements)));m_resultsTable->setItem(r,8,new QTableWidgetItem(QString::number(x.missingQuantity)));const int sourceCount=x.collectionSourceCount>0?x.collectionSourceCount:x.sources.size();auto*source=new QPushButton(sourceCount==0?QStringLiteral("Loose only"):QStringLiteral("%1 item(s)").arg(sourceCount),m_resultsTable);source->setEnabled(sourceCount>0);connect(source,&QPushButton::clicked,this,[this,x]{showSources(x);});m_resultsTable->setCellWidget(r,9,source);auto*a=new QComboBox(m_resultsTable);a->addItem(QStringLiteral("Actions..."));a->addItem(QStringLiteral("View Set Details"),QStringLiteral("details"));a->addItem(QStringLiteral("View Missing Parts"),QStringLiteral("missing"));a->addItem(QStringLiteral("Create Build"),QStringLiteral("build"));if(x.setCatalogId<=0){a->setItemData(1,0,Qt::UserRole-1);a->setItemData(3,0,Qt::UserRole-1);a->setItemData(1,QStringLiteral("This Set is not available in the local catalog."),Qt::ToolTipRole);a->setItemData(3,QStringLiteral("Update the local Sets Catalog before creating this Build."),Qt::ToolTipRole);}connect(a,&QComboBox::currentIndexChanged,this,[this,a,x](int n){if(n<=0)return;const QString action=a->itemData(n).toString();a->setCurrentIndex(0);if(action=="details"&&x.setCatalogId>0)showDetails(x.setCatalogId);else if(action=="missing")showMissingParts(x);else if(action=="build"&&x.setCatalogId>0){if(x.usesCollection()&&QMessageBox::warning(this,QStringLiteral("Advisory Collection Sources"),QStringLiteral("This discovery result includes advisory Parts from opted-in Collection Sets. Creating the Build does not disassemble those Sets or make their Parts available to Build From Stock."),QMessageBox::Ok|QMessageBox::Cancel)!=QMessageBox::Ok)return;emit createBuildRequested(x.setCatalogId,QStringLiteral("Stock"));}});m_resultsTable->setCellWidget(r,10,a);if(!x.imageUrl.isEmpty())m_images->requestSetImage(x.setNumber,x.imageUrl);} }
    else {m_resultsTable->setColumnCount(8);m_resultsTable->setHorizontalHeaderLabels({QStringLiteral("Image"),QStringLiteral("Set #"),QStringLiteral("Name"),QStringLiteral("Year"),QStringLiteral("Theme"),QStringLiteral("Parts"),QStringLiteral("Selected Parts Match"),QStringLiteral("Actions")});
    for(int i=begin;i<end;++i){const auto&x=m_results.at(i);int r=m_resultsTable->rowCount();m_resultsTable->insertRow(r);auto*image=new QTableWidgetItem;image->setTextAlignment(Qt::AlignCenter);m_resultsTable->setItem(r,0,image);auto*num=new QTableWidgetItem(x.setNumber);num->setData(Qt::UserRole,x.setCatalogId);m_resultsTable->setItem(r,1,num);m_resultsTable->setItem(r,2,new QTableWidgetItem(x.name));m_resultsTable->setItem(r,3,new QTableWidgetItem(QString::number(x.year)));m_resultsTable->setItem(r,4,new QTableWidgetItem(x.themeName));m_resultsTable->setItem(r,5,new QTableWidgetItem(QString::number(x.catalogPartCount)));m_resultsTable->setItem(r,6,new QTableWidgetItem(QStringLiteral("%1 / %2").arg(x.matchedCriteria).arg(x.totalCriteria)));auto*a=new QComboBox(m_resultsTable);a->addItem(QStringLiteral("Actions..."));a->addItem(QStringLiteral("View Set Details"),QStringLiteral("details"));a->addItem(QStringLiteral("Create Build"),QStringLiteral("build"));connect(a,&QComboBox::currentIndexChanged,this,[this,a,id=x.setCatalogId](int n){if(n<=0)return;QString action=a->itemData(n).toString();a->setCurrentIndex(0);if(action==QStringLiteral("details"))showDetails(id);else emit createBuildRequested(id,QStringLiteral("Stock"));});m_resultsTable->setCellWidget(r,7,a);if(!x.imageUrl.isEmpty())m_images->requestSetImage(x.setNumber,x.imageUrl);}
    }
    m_pageLabel->setText(QStringLiteral("Page %1 of %2").arg(m_page+1).arg(pages));m_previous->setEnabled(m_page>0);m_next->setEnabled(m_page+1<pages);updateControls();
}

void WhatCanIBuildWidget::showDetails(int id){SetDetailsDialog dialog(id,m_workspaceContext,this,m_remoteReads,m_remoteCollection);connect(&dialog,&SetDetailsDialog::createBuildRequested,this,[this,id](int,const QString&){emit createBuildRequested(id,QStringLiteral("Stock"));});connect(&dialog,&SetDetailsDialog::compositionChanged,this,&WhatCanIBuildWidget::invalidateCatalog);dialog.exec();}
void WhatCanIBuildWidget::showMissingParts(const InventoryBuildabilitySetResult&x){if(m_remoteReads){requestRemoteDetails(x,false);return;}QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("Missing Parts — %1").arg(x.setNumber));dialog.resize(760,420);auto*l=new QVBoxLayout(&dialog);auto*t=new QTableWidget(&dialog);t->setColumnCount(7);t->setHorizontalHeaderLabels({"Part #","Name","Color","Required","Loose","Collection","Missing"});for(const auto&r:x.requirements)if(r.missing>0){int row=t->rowCount();t->insertRow(row);t->setItem(row,0,new QTableWidgetItem(r.partNumber));t->setItem(row,1,new QTableWidgetItem(r.partName));t->setItem(row,2,new QTableWidgetItem(r.colorName));t->setItem(row,3,new QTableWidgetItem(QString::number(r.required)));t->setItem(row,4,new QTableWidgetItem(QString::number(r.looseUsed)));t->setItem(row,5,new QTableWidgetItem(QString::number(r.collectionUsed)));t->setItem(row,6,new QTableWidgetItem(QString::number(r.missing)));}t->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);l->addWidget(t);auto*b=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);connect(b,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);l->addWidget(b);dialog.exec();}
void WhatCanIBuildWidget::showSources(const InventoryBuildabilitySetResult&x){if(m_remoteReads){requestRemoteDetails(x,true);return;}QStringList rows;for(const auto&s:x.sources)rows<<QStringLiteral("%1 (%2) — %3 piece(s)").arg(s.label,s.state).arg(s.piecesUsed);QMessageBox::information(this,QStringLiteral("Advisory Collection Sources"),rows.join('\n'));}

void WhatCanIBuildWidget::requestRemoteDetails(const InventoryBuildabilitySetResult&x,bool sourcesOnly){if(!m_remoteReads||!m_remoteReads->isAvailableFor(QStringLiteral("buildability.inventory.details")))return;auto*dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(sourcesOnly?QStringLiteral("Advisory Collection Sources — %1").arg(x.setNumber):QStringLiteral("Missing Parts — %1").arg(x.setNumber));dialog->resize(800,440);auto*layout=new QVBoxLayout(dialog);auto*status=new QLabel(QStringLiteral("Requesting current details from BrickSuite Host..."),dialog);layout->addWidget(status);auto*table=new QTableWidget(dialog);table->setColumnCount(sourcesOnly?3:8);table->setHorizontalHeaderLabels(sourcesOnly?QStringList{"Collection Item","State","Pieces"}:QStringList{"Part #","Name","Color","Required","Loose Available","Loose Used","Collection","Missing"});table->horizontalHeader()->setSectionResizeMode(sourcesOnly?0:1,QHeaderView::Stretch);layout->addWidget(table);auto*paging=new QHBoxLayout;auto*previous=new QPushButton(QStringLiteral("Previous"),dialog);auto*pageLabel=new QLabel(QStringLiteral("Page 1 of 1"),dialog);auto*next=new QPushButton(QStringLiteral("Next"),dialog);previous->setVisible(!sourcesOnly);pageLabel->setVisible(!sourcesOnly);next->setVisible(!sourcesOnly);paging->addStretch();paging->addWidget(previous);paging->addWidget(pageLabel);paging->addWidget(next);layout->addLayout(paging);auto*buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);layout->addWidget(buttons);const quint64 generation=++m_remoteDetailsGeneration;const int workspaceId=m_workspaceContext.currentWorkspaceId();auto load=std::make_shared<std::function<void(int)>>();QPointer<QDialog> guard(dialog);*load=[this,guard,status,table,previous,next,pageLabel,load,generation,workspaceId,x,sourcesOnly](int page){if(!guard||generation!=m_remoteDetailsGeneration)return;RemoteBuildabilityDto::DetailsRequest request;request.workspaceId=workspaceId;request.setNumber=x.setNumber;request.includeCollection=m_includeCollection->isChecked();request.paging={page,250};status->setText(QStringLiteral("Requesting current details from BrickSuite Host..."));previous->setEnabled(false);next->setEnabled(false);m_remoteReads->buildabilityDetails(request,guard,[this,guard,status,table,previous,next,pageLabel,load,generation,workspaceId,sourcesOnly](AsyncReadResult<RemoteBuildabilityDto::DetailsResponse> outcome){if(!guard||generation!=m_remoteDetailsGeneration||workspaceId!=m_workspaceContext.currentWorkspaceId())return;if(!outcome.succeeded()){status->setText(QStringLiteral("Unable to load details: %1").arg(outcome.message));return;}const auto&value=*outcome.value;table->setRowCount(0);if(sourcesOnly){for(const auto&source:value.sources){const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(source.displayLabel));table->setItem(row,1,new QTableWidgetItem(source.state));table->setItem(row,2,new QTableWidgetItem(QString::number(source.piecesUsed)));}}else{for(const auto&r:value.requirements)if(r.missingQuantity>0){const int row=table->rowCount();table->insertRow(row);const QStringList cells{r.partNumber,r.partDescription,r.colorName,QString::number(r.requiredQuantity),QString::number(r.looseAvailableQuantity),QString::number(r.looseUsedQuantity),QString::number(r.collectionUsedQuantity),QString::number(r.missingQuantity)};for(int c=0;c<cells.size();++c)table->setItem(row,c,new QTableWidgetItem(cells.at(c)));}}const int pages=qMax(1,(value.totalCount+value.pageSize-1)/value.pageSize);pageLabel->setText(QStringLiteral("Page %1 of %2").arg(value.page).arg(pages));previous->setEnabled(value.page>1);next->setEnabled(value.page<pages);previous->disconnect();next->disconnect();connect(previous,&QPushButton::clicked,guard,[load,page=value.page]{(*load)(page-1);});connect(next,&QPushButton::clicked,guard,[load,page=value.page]{(*load)(page+1);});status->setText(sourcesOnly?QStringLiteral("Current Host Collection sources."):QStringLiteral("Current Host requirement availability."));});};connect(dialog,&QObject::destroyed,this,[this,generation]{if(m_remoteDetailsGeneration==generation)++m_remoteDetailsGeneration;});dialog->show();(*load)(1);}
void WhatCanIBuildWidget::updateControls(){m_addButton->setEnabled(m_criteria.size()<20&&m_resolvedPart.has_value());m_searchButton->setEnabled(!m_searchOutstanding&&!m_criteria.isEmpty());const bool validYears=m_yearFromSpin->value()==0||m_yearToSpin->value()==0||m_yearFromSpin->value()<=m_yearToSpin->value();m_inventorySearchButton->setEnabled(!m_searchOutstanding&&validYears);m_inventorySearchButton->setToolTip(validYears?QString():QStringLiteral("Year From cannot be later than Year To."));m_addButton->setToolTip(m_criteria.size()>=20?QStringLiteral("A search can contain at most 20 selected Part criteria."):QString());}
