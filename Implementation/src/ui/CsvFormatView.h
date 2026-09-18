#pragma once

#include <gui/Label.h>
#include <gui/TextEdit.h>
#include <gui/VerticalLayout.h>
#include <gui/View.h>

class CsvFormatView final : public gui::View
{
    gui::Label _introduction;
    gui::Label _exampleHeading;
    gui::TextEdit _example;
    gui::Label _requirementsHeading;
    gui::TextEdit _requirements;
    gui::VerticalLayout _layout;

public:
    CsvFormatView()
        : _introduction(tr("csvFormatIntro"))
        , _exampleHeading(tr("csvFormatExampleHeading"),
                          gui::Font::ID::SystemBold)
        , _example(gui::TextEdit::HorizontalScroll::No,
                   gui::TextEdit::Events::DoNotSend, true)
        , _requirementsHeading(tr("csvReqHeading"),
                               gui::Font::ID::SystemBold)
        , _requirements(gui::TextEdit::HorizontalScroll::No,
                        gui::TextEdit::Events::DoNotSend, true)
        , _layout(7)
    {
        _example.setText(tr("csvFormatExample"));
        _example.setFontName("Consolas");
        _example.setSizeLimits(0, gui::Control::Limit::None,
                               95, gui::Control::Limit::Fixed);

        _requirements.setText(tr("csvReqBody"));
        _requirements.setSizeLimits(0, gui::Control::Limit::None,
                                    185, gui::Control::Limit::UseAsMin);

        _layout << _introduction;
        _layout.appendSpace(8);
        _layout << _exampleHeading << _example;
        _layout.appendSpace(8);
        _layout << _requirementsHeading << _requirements;
        setLayout(&_layout);
    }
};
