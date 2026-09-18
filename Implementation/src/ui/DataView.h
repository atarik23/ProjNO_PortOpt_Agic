#pragma once

#include "../AppState.h"
#include "CsvFormatView.h"

#include <gui/Alert.h>
#include <gui/Button.h>
#include <gui/Dialog.h>
#include <gui/FileDialog.h>
#include <gui/HorizontalLayout.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/Panel.h>
#include <gui/TextEdit.h>
#include <gui/VerticalLayout.h>
#include <gui/View.h>

#include <filesystem>
#include <functional>
#include <string>
#include <utility>

class DataView : public gui::View
{
    AppState& _state;
    gui::Label _description;
    gui::Label _fileLabel;
    gui::LineEdit _filePath;
    gui::Button _loadCsv;
    gui::Button _loadSample;
    gui::Button _loadRealSample;
    gui::Button _showCsvFormat;
    gui::HorizontalLayout _fileLayout;
    gui::HorizontalLayout _buttonLayout;
    gui::TextEdit _summary;
    gui::VerticalLayout _layout;
    std::function<void()> _onDataLoaded;

    void loadPath(const td::String& fileName)
    {
        std::string error;
        if (!_state.loadReturns(fileName.c_str(), error))
        {
            gui::Alert::show(tr("loadError"), td::String(error.c_str()));
            return;
        }

        _filePath.setText(fileName);
        const std::string summary = _state.summary();
        _summary.setText(td::String(summary.c_str()));

        if (_onDataLoaded)
            _onDataLoaded();
    }

public:
    explicit DataView(AppState& state)
        : _state(state)
        , _description(tr("dataInstructions"))
        , _fileLabel(tr("selectedFile"))
        , _loadCsv(tr("loadCsv"))
        , _loadSample(tr("loadSample"))
        , _loadRealSample(tr("loadRealSample"))
        , _showCsvFormat(tr("showCsvFormat"))
        , _fileLayout(2)
        , _buttonLayout(5)
        , _layout(4)
    {
        _filePath.setAsReadOnly();
        _summary.setAsReadOnly();
        _summary.setText(tr("noData"));
        _loadCsv.setType(gui::Button::Type::Constructive);

        _loadCsv.setToolTip(tr("loadCsvTooltip"));
        _loadSample.setToolTip(tr("loadSampleTooltip"));
        _loadRealSample.setToolTip(tr("loadRealSampleTooltip"));
        _showCsvFormat.setToolTip(tr("csvFormatTooltip"));

        _fileLayout << _fileLabel << _filePath;
        _buttonLayout << _loadCsv << _loadSample << _loadRealSample << _showCsvFormat;
        _buttonLayout.appendSpacer();
        _layout << _description << _fileLayout << _buttonLayout << _summary;
        setLayout(&_layout);

        _loadCsv.onClick([this]()
        {
            constexpr td::UINT4 dialogID = 1001;
            gui::OpenFileDialog::show(this, tr("openCsv"),
                                      {{tr("csvFiles"), "*.csv"}}, dialogID,
                                      [this](gui::FileDialog* dialog)
            {
                if (dialog->getStatus() == gui::FileDialog::Status::OK)
                    loadPath(dialog->getFileName());
            });
        });

        _loadSample.onClick([this]()
        {
            loadPath(gui::getResFileName(":sampleReturns"));
        });

        _loadRealSample.onClick([this]()
        {
            const td::String sampleFile = gui::getResFileName(":sampleReturns");
            const std::filesystem::path realMarketFile =
                std::filesystem::path(sampleFile.c_str()).parent_path() /
                "real_market_returns.csv";

            loadPath(td::String(realMarketFile.string().c_str()));
        });

        _showCsvFormat.onClick([this]()
        {
            constexpr td::UINT4 dialogID = 1002;
            gui::Panel::show<CsvFormatView>(
                this,
                tr("csvFormatTitle"),
                gui::Size(640, 430),
                dialogID,
                {{gui::Dialog::Button::ID::OK,
                  tr("Ok"),
                  gui::Button::Type::Default}},
                std::function<void(gui::Dialog*)>{[](gui::Dialog*) {}});
        });
    }

    void setOnDataLoaded(std::function<void()> callback)
    {
        _onDataLoaded = std::move(callback);
    }
};
