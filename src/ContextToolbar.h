// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

class ContextToolbar final : public ju::Component
{
public:
    enum class Edit { cut, remove, copy, paste };
    static constexpr std::array edits { Edit::cut, Edit::remove, Edit::copy, Edit::paste };
    static constexpr int pageCount = 2;

    ContextToolbar()
    {
        setMouseClickGrabsKeyboardFocus(false);
        for (size_t i = 0; i < edits.size(); ++i)
        {
            auto& button = buttons[i];
            button.setWantsKeyboardFocus(false);
            button.setMouseClickGrabsKeyboardFocus(false);
            button.onClick = [this, i] { if (onEdit) onEdit(edits[i]); };
            addChildComponent(button);
        }
    }

    void open(ju::Point<int> pointer, ju::Rectangle<int> available)
    {
        const auto size = ju::Rectangle<int>(0, 0, 324, 108).getIntersection(available.withZeroOrigin());
        panel = size.withPosition(pointer + ju::Point<int>(12, 12)).constrainedWithin(available);
        page = 0;
        dismissOnRelease = false;
        showPage();
        setVisible(true);
        toFront(false);
    }

    void setEditEnabled(size_t index, bool enabled) { buttons[index].setEnabled(enabled); }

    std::function<void(Edit)> onEdit;
    std::function<void()> onDismiss;

private:
    friend class MainGui;

    class EditButton final : public ju::Button
    {
    public:
        EditButton(ContextToolbar& toolbar, Edit edit, const char* label)
            : ju::Button(label), owner(toolbar), action(edit) {}

        void mouseDown(const ju::MouseEvent& event) override
        {
            leftPress = event.mods.isLeftButtonDown();
            if (leftPress)
                ju::Button::mouseDown(event);
            else
                owner.mouseDown(event.getEventRelativeTo(&owner));
        }

        void mouseDrag(const ju::MouseEvent& event) override
        {
            if (leftPress)
                ju::Button::mouseDrag(event);
        }

        void mouseUp(const ju::MouseEvent& event) override
        {
            if (leftPress)
                ju::Button::mouseUp(event);
            else
                owner.mouseUp(event.getEventRelativeTo(&owner));
        }

        void mouseWheelMove(const ju::MouseEvent& event, const ju::MouseWheelDetails& wheel) override
        {
            owner.mouseWheelMove(event.getEventRelativeTo(&owner), wheel);
        }

        void paintButton(ju::Graphics& g, bool highlighted, bool pressed) override
        {
            auto fill = ju::Colour(0xff334155);
            if (isEnabled() && (highlighted || pressed))
                fill = ju::Colour(pressed ? 0xff466382 : 0xff3c536d);
            g.setColour(fill);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

            auto bounds = getLocalBounds().reduced(3);
            auto iconArea = bounds.removeFromTop(bounds.getHeight() * 2 / 3);
            auto iconSize = ju::jmin(30.0f, static_cast<float>(ju::jmin(iconArea.getWidth(), iconArea.getHeight())));
            auto icon = iconArea.toFloat().withSizeKeepingCentre(iconSize, iconSize);
            ju::Path lines;
            switch (action)
            {
                case Edit::cut:
                    lines.addEllipse(2, 3, 7, 7);
                    lines.addEllipse(2, 16, 7, 7);
                    lines.startNewSubPath(8, 9); lines.lineTo(22, 21);
                    lines.startNewSubPath(8, 17); lines.lineTo(22, 5);
                    break;
                case Edit::remove:
                    lines.startNewSubPath(2, 6); lines.lineTo(22, 6);
                    lines.startNewSubPath(8, 6); lines.lineTo(8, 2); lines.lineTo(16, 2); lines.lineTo(16, 6);
                    lines.startNewSubPath(5, 6); lines.lineTo(6, 22); lines.lineTo(18, 22); lines.lineTo(19, 6);
                    lines.startNewSubPath(10, 10); lines.lineTo(10, 18);
                    lines.startNewSubPath(14, 10); lines.lineTo(14, 18);
                    break;
                case Edit::copy:
                    lines.addRoundedRectangle(8, 8, 14, 14, 2);
                    lines.startNewSubPath(4, 16); lines.lineTo(2, 16); lines.lineTo(2, 2);
                    lines.lineTo(16, 2); lines.lineTo(16, 4);
                    break;
                case Edit::paste:
                    lines.addRoundedRectangle(8, 2, 8, 5, 1);
                    lines.startNewSubPath(8, 4); lines.lineTo(4, 4); lines.lineTo(4, 22);
                    lines.lineTo(20, 22); lines.lineTo(20, 4); lines.lineTo(16, 4);
                    lines.startNewSubPath(8, 12); lines.lineTo(16, 12);
                    lines.startNewSubPath(8, 16); lines.lineTo(16, 16);
                    break;
            }

            g.setColour(ju::Colours::white.withAlpha(isEnabled() ? 1.0f : 0.43f));
            auto transform = ju::AffineTransform::scale(iconSize / 24.0f).translated(icon.getX(), icon.getY());
            g.strokePath(lines, ju::PathStrokeType(1.9f, ju::PathStrokeType::curved, ju::PathStrokeType::rounded), transform);
            g.setFont(ju::FontOptions(14.0f));
            g.drawFittedText(getButtonText(), bounds, ju::Justification::centred, 1);
        }

    private:
        ContextToolbar& owner;
        Edit action;
        bool leftPress = false;
    };

    void showPage()
    {
        auto row = panel.reduced(8).withTrimmedTop(24);
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            auto& button = buttons[i];
            button.setVisible(page == 0);
            button.setBounds(row.removeFromLeft(row.getWidth() / static_cast<int>(buttons.size() - i)).reduced(2));
        }
        repaint(panel);
    }

    void paint(ju::Graphics& g) override
    {
        const auto frame = panel.toFloat().reduced(0.5f);
        g.setColour(ju::Colour(0xff1e293b));
        g.fillRoundedRectangle(frame, 8.0f);
        g.setColour(ju::Colour(0xff64748b));
        g.drawRoundedRectangle(frame, 8.0f, 1.0f);

        auto area = panel.reduced(10, 6);
        auto heading = area.removeFromTop(22);
        g.setColour(ju::Colours::white);
        g.setFont(ju::FontOptions(15.0f, ju::Font::bold));
        g.drawText(ju::String(page + 1) + " / " + ju::String(pageCount), heading.removeFromRight(48),
                   ju::Justification::centredRight);
        g.drawText(page == 0 ? "Edit" : "Wheel test", heading, ju::Justification::centredLeft);
        if (page == 1)
        {
            g.setFont(ju::FontOptions(14.0f));
            g.drawFittedText("Test page only. Scroll to return to Edit.", area, ju::Justification::centred, 2);
        }
    }

    void mouseDown(const ju::MouseEvent& event) override
    {
        dismissOnRelease = !panel.contains(event.getPosition()) || event.mods.isMiddleButtonDown()
            || event.mods.isRightButtonDown();
    }

    void mouseUp(const ju::MouseEvent&) override
    {
        if (dismissOnRelease && onDismiss)
            onDismiss();
        dismissOnRelease = false;
    }

    void mouseWheelMove(const ju::MouseEvent&, const ju::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY != 0.0f && !wheel.isInertial)
        {
            page = (page + pageCount + (wheel.deltaY < 0.0f ? 1 : -1)) % pageCount;
            showPage();
        }
    }

    std::array<EditButton, 4> buttons {{
        { *this, Edit::cut, "Cut" }, { *this, Edit::remove, "Delete" },
        { *this, Edit::copy, "Copy" }, { *this, Edit::paste, "Paste" }
    }};
    ju::Rectangle<int> panel;
    int page = 0;
    bool dismissOnRelease = false;
};
