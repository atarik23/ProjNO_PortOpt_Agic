#pragma once

#include "../AppState.h"

#include <gui/Alert.h>
#include <gui/Button.h>
#include <gui/FileDialog.h>
#include <gui/HorizontalLayout.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/TextEdit.h>
#include <gui/VerticalLayout.h>
#include <gui/View.h>

#include <string>

class DataView : public gui::View
{
    AppState& _state;
    gui::Label _description;
    gui::Label _fileLabel;
    gui::LineEdit _filePath;
    gui::Button _loadCsv;
    gui::Button _loadSample;
    gui::HorizontalLayout _fileLayout;
    gui::HorizontalLayout _buttonLayout;
    gui::TextEdit _summary;
    gui::VerticalLayout _layout;

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
    }

public:
    explicit DataView(AppState& state)
        : _state(state)
        , _description(tr("dataInstructions"))
        , _fileLabel(tr("selectedFile"))
        , _loadCsv(tr("loadCsv"))
        , _loadSample(tr("loadSample"))
        , _fileLayout(2)
        , _buttonLayout(3)
        , _layout(4)
    {
        _filePath.setAsReadOnly();
        _summary.setAsReadOnly();
        _summary.setText(tr("noData"));
        _loadCsv.setType(gui::Button::Type::Constructive);

        _fileLayout << _fileLabel << _filePath;
        _buttonLayout << _loadCsv << _loadSample;
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
    }
};

