// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

class ContextToolbar final : public ju::Component
{
public:
    enum class Command { cut, remove, copy, paste, left, right, up, down, finer, coarser };
    static constexpr int movePage = 0, editPage = 1;
    static constexpr std::array moves { Command::left, Command::right, Command::up, Command::down };
    static constexpr std::array edits { Command::cut, Command::copy, Command::paste, Command::remove };
    static constexpr int pageCount = 2;

    ContextToolbar()
    {
        setMouseClickGrabsKeyboardFocus(false);
        for (size_t i = 0; i < edits.size(); ++i)
        {
            configureButton(buttons[i], edits[i]);
            configureButton(moveButtons[i], moves[i]);
        }
        configureButton(finer, Command::finer);
        configureButton(coarser, Command::coarser);
        finer.setTitle("Finer snap");
        coarser.setTitle("Coarser snap");
    }

    void open(ju::Point<int> pointer, ju::Rectangle<int> available)
    {
        const auto size = ju::Rectangle<int>(0, 0, 324, 144).getIntersection(available.withZeroOrigin());
        panel = size.withPosition(pointer + ju::Point<int>(12, 12)).constrainedWithin(available);
        page = movePage;
        dismissOnRelease = false;
        showPage();
        setVisible(true);
        toFront(false);
    }

    void setEditEnabled(size_t index, bool enabled) { buttons[index].setEnabled(enabled); }

    void setMoveEnabled(size_t index, bool enabled) { moveButtons[index].setEnabled(enabled); }

    void setSnap(ju::String text, bool enabled)
    {
        finer.setEnabled(enabled);
        coarser.setEnabled(enabled);
        if (snapText != text)
        {
            snapText = std::move(text);
            repaint(snapBounds);
        }
    }

    std::function<void(Command)> onAction;
    std::function<void()> onDismiss;

private:
    friend class MainGui;

    class EditButton final : public ju::Button
    {
    public:
        EditButton(ContextToolbar& toolbar, Command edit, const char* label)
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

            if (action == Command::finer || action == Command::coarser)
            {
                g.setColour(ju::Colours::white.withAlpha(isEnabled() ? 1.0f : 0.43f));
                g.setFont(ju::FontOptions(18.0f));
                g.drawText(getButtonText(), getLocalBounds(), ju::Justification::centred);
                return;
            }

            auto bounds = getLocalBounds().reduced(3);
            auto iconArea = bounds.removeFromTop(bounds.getHeight() * 2 / 3);
            auto iconSize = ju::jmin(30.0f, static_cast<float>(ju::jmin(iconArea.getWidth(), iconArea.getHeight())));
            auto icon = iconArea.toFloat().withSizeKeepingCentre(iconSize, iconSize);
            ju::Path lines;
            switch (action)
            {
                case Command::cut:
                    lines.addEllipse(2, 3, 7, 7);
                    lines.addEllipse(2, 16, 7, 7);
                    lines.startNewSubPath(8, 9); lines.lineTo(22, 21);
                    lines.startNewSubPath(8, 17); lines.lineTo(22, 5);
                    break;
                case Command::remove:
                    lines.startNewSubPath(2, 6); lines.lineTo(22, 6);
                    lines.startNewSubPath(8, 6); lines.lineTo(8, 2); lines.lineTo(16, 2); lines.lineTo(16, 6);
                    lines.startNewSubPath(5, 6); lines.lineTo(6, 22); lines.lineTo(18, 22); lines.lineTo(19, 6);
                    lines.startNewSubPath(10, 10); lines.lineTo(10, 18);
                    lines.startNewSubPath(14, 10); lines.lineTo(14, 18);
                    break;
                case Command::copy:
                    lines.addRoundedRectangle(8, 8, 14, 14, 2);
                    lines.startNewSubPath(4, 16); lines.lineTo(2, 16); lines.lineTo(2, 2);
                    lines.lineTo(16, 2); lines.lineTo(16, 4);
                    break;
                case Command::paste:
                    lines.addRoundedRectangle(8, 2, 8, 5, 1);
                    lines.startNewSubPath(8, 4); lines.lineTo(4, 4); lines.lineTo(4, 22);
                    lines.lineTo(20, 22); lines.lineTo(20, 4); lines.lineTo(16, 4);
                    lines.startNewSubPath(8, 12); lines.lineTo(16, 12);
                    lines.startNewSubPath(8, 16); lines.lineTo(16, 16);
                    break;
                case Command::left: case Command::right: case Command::up: case Command::down:
                {
                    lines.startNewSubPath(20, 12); lines.lineTo(4, 12);
                    lines.startNewSubPath(11, 5); lines.lineTo(4, 12); lines.lineTo(11, 19);
                    const float quarter = ju::MathConstants<float>::halfPi;
                    const float angle = action == Command::up ? quarter : action == Command::right ? 2 * quarter
                        : action == Command::down ? -quarter : 0.0f;
                    lines.applyTransform(ju::AffineTransform::rotation(angle, 12, 12));
                    break;
                }
                case Command::finer: case Command::coarser: break;
            }

            g.setColour(ju::Colours::white.withAlpha(isEnabled() ? 1.0f : 0.43f));
            auto transform = ju::AffineTransform::scale(iconSize / 24.0f).translated(icon.getX(), icon.getY());
            g.strokePath(lines, ju::PathStrokeType(1.9f, ju::PathStrokeType::curved, ju::PathStrokeType::rounded), transform);
            g.setFont(ju::FontOptions(14.0f));
            g.drawFittedText(getButtonText(), bounds, ju::Justification::centred, 1);
        }

    private:
        ContextToolbar& owner;
        Command action;
        bool leftPress = false;
    };

    void configureButton(ju::Button& button, Command action)
    {
        button.setWantsKeyboardFocus(false);
        button.setMouseClickGrabsKeyboardFocus(false);
        button.onClick = [this, action] { if (onAction) onAction(action); };
        addChildComponent(button);
    }

    void showPage()
    {
        auto row = panel.reduced(8).withTrimmedTop(24);
        snapBounds = row.removeFromTop(33).reduced(2);
        auto snapRow = snapBounds;
        finer.setBounds(snapRow.removeFromLeft(52));
        coarser.setBounds(snapRow.removeFromRight(52));
        finer.setVisible(page == movePage);
        coarser.setVisible(page == movePage);
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            auto bounds = row.removeFromLeft(row.getWidth() / static_cast<int>(buttons.size() - i)).reduced(2);
            buttons[i].setBounds(bounds);
            moveButtons[i].setBounds(bounds);
            buttons[i].setVisible(page == editPage);
            moveButtons[i].setVisible(page == movePage);
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
        g.drawText(page == movePage ? "Move" : "Edit", heading, ju::Justification::centredLeft);
        if (page == movePage)
        {
            g.setFont(ju::FontOptions(14.0f));
            g.drawFittedText("SNAP  " + snapText, snapBounds.reduced(54, 0), ju::Justification::centred, 1);
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
        { *this, Command::cut, "Cut" }, { *this, Command::copy, "Copy" },
        { *this, Command::paste, "Paste" }, { *this, Command::remove, "Delete" }
    }};
    std::array<EditButton, 4> moveButtons {{
        { *this, Command::left, "Left" }, { *this, Command::right, "Right" },
        { *this, Command::up, "Up" }, { *this, Command::down, "Down" }
    }};
    EditButton finer { *this, Command::finer, "‹" }, coarser { *this, Command::coarser, "›" };
    ju::Rectangle<int> panel, snapBounds;
    ju::String snapText;
    int page = 0;
    bool dismissOnRelease = false;
};
