// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

class EffectsEditor final : public ju::Component
{
public:
    EffectsEditor(std::function<te::Edit*()> getEdit, std::function<void()> onClose)
        : edit(std::move(getEdit))
    {
        for (auto* label : { &title, &bypass, &mixGuide })
        {
            label->setColour(ju::Label::textColourId, ju::Colours::white);
            addAndMakeVisible(*label);
        }
        title.setFont(ju::FontOptions(20.0f, ju::Font::bold));
        bypass.setFont(ju::FontOptions(16.0f, ju::Font::bold));
        bypass.setJustificationType(ju::Justification::centred);
        mixGuide.setFont(ju::FontOptions(14.0f));
        addAndMakeVisible(closeButton);
        closeButton.onClick = std::move(onClose);
        setTitle("Effects Editor");
    }

    bool open(te::EditItemID trackID, te::EditItemID pluginID)
    {
        const Target requested { trackID, pluginID };
        auto* plugin = resolve(requested).second;
        if (!plugin)
            return false;

        const auto* definitions = parameterDefinitions(*plugin);
        if (!definitions)
            return false;

        close();
        target = requested;
        for (const auto& definition : *definitions)
        {
            auto parameter = plugin->getAutomatableParameterByID(definition.id);
            if (!parameter)
                continue;
            auto& control = *controls.emplace_back(std::make_unique<Control>(definition, *parameter));
            addAndMakeVisible(control);
            auto& slider = control.slider;
            slider.onDragStart = [this, &control] {
                if (parameterFor(control))
                    edit()->getUndoManager().beginNewTransaction(control.name.getText());
            };
            slider.onDragEnd = [this] {
                if (auto* project = edit())
                    project->getUndoManager().beginNewTransaction();
            };
            slider.onValueChange = [this, &control] {
                auto parameter = parameterFor(control);
                if (!parameter)
                    return;
                const auto value = control.mapRatio(static_cast<float>(control.slider.getValue()));
                const bool dragging = control.slider.getThumbBeingDragged() >= 0;
                auto& history = edit()->getUndoManager();
                if (!dragging)
                    history.beginNewTransaction(control.name.getText());
                parameter->setParameter(parameter->getValueRange().clipValue(value), ju::sendNotification);
                if (!dragging)
                    history.beginNewTransaction();
                control.update(*parameter);
            };
        }
        mixGuide.setVisible(dynamic_cast<te::ReverbPlugin*>(plugin) != nullptr);
        refresh();
        resized();
        return true;
    }

    static bool applyHistory(te::Edit& project, bool redo)
    {
        struct ChangedProperties final : ju::ValueTree::Listener
        {
            void valueTreePropertyChanged(ju::ValueTree& tree, const ju::Identifier& property) override
            {
                if (tree.hasType(te::IDs::PLUGIN))
                    changes.emplace_back(tree, property);
            }

            std::vector<std::pair<ju::ValueTree, ju::Identifier>> changes;
        } observed;

        bool applied = false;
        {
            project.state.addListener(&observed);
            const ju::ScopeGuard detach { [&] { project.state.removeListener(&observed); } };
            auto& history = project.getUndoManager();
            applied = redo ? history.redo() : history.undo();
        }

        // Tracktion restores the attached ValueTree property on undo. Its parameter listener
        // deliberately leaves the explicit value alone because normal changes may be from automation.
        for (auto& [state, property] : observed.changes)
        {
            auto plugin = te::findPluginForState(project, state);
            const auto* definitions = plugin ? parameterDefinitions(*plugin) : nullptr;
            if (!definitions)
                continue;
            const auto definition = std::find_if(definitions->begin(), definitions->end(),
                [&property](const auto& candidate) { return candidate.property == property; });
            if (definition == definitions->end())
                continue;
            if (auto parameter = plugin->getAutomatableParameterByID(definition->id))
            {
                // Undo of a first edit removes the property. Temporarily materialize the native
                // default so refreshing the parameter cannot create an undo step and discard Redo.
                const auto defaultValue = state.hasProperty(property) ? std::nullopt : parameter->getDefaultValue();
                if (defaultValue)
                    state.setProperty(property, *defaultValue, nullptr);
                parameter->updateFromAttachedValue();
                if (defaultValue)
                    state.removeProperty(property, nullptr);
            }
        }
        return applied;
    }

    bool isOpen() const { return target.has_value(); }

    void close()
    {
        target.reset();
        for (const auto& control : controls)
        {
            if (control->slider.getThumbBeingDragged() >= 0)
                if (auto* project = edit())
                    project->getUndoManager().beginNewTransaction();
            control->slider.onDragEnd = nullptr;
        }
        controls.clear();
    }

    bool refresh()
    {
        if (!target)
            return false;
        const auto [track, plugin] = resolve(*target);
        if (!plugin)
            return false;

        const auto type = plugin->getPluginType();
        int occurrence = 0;
        for (auto* insert : track->pluginList.getPlugins())
        {
            if (insert->getPluginType() == type)
                ++occurrence;
            if (insert == plugin)
                break;
        }
        title.setText(plugin->getName() + " " + ju::String(occurrence) + "  |  " + track->getName(),
                      ju::dontSendNotification);
        const bool enabled = plugin->isEnabled();
        bypass.setText(enabled ? "ACTIVE" : "BYPASSED", ju::dontSendNotification);
        bypass.setColour(ju::Label::textColourId, enabled ? ju::Colours::limegreen : ju::Colours::yellow);
        for (const auto& control : controls)
            if (auto parameter = plugin->getAutomatableParameterByID(control->definition.id))
                control->update(*parameter);
        return true;
    }

    void setActive(bool active)
    {
        if (active != highlighted)
        {
            highlighted = active;
            repaint();
        }
    }

    void paint(ju::Graphics& graphics) override
    {
        graphics.fillAll(ju::Colours::black);
        graphics.setColour(highlighted ? ju::Colours::white : ju::Colours::dimgrey);
        graphics.drawRect(getLocalBounds(), 2);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(9, 6);
        auto header = area.removeFromTop(34);
        closeButton.setBounds(header.removeFromRight(100));
        bypass.setBounds(header.removeFromRight(116));
        title.setBounds(header);
        if (mixGuide.isVisible())
            mixGuide.setBounds(area.removeFromBottom(18));
        const auto rows = (controls.size() + 2) / 3;
        for (size_t index = 0; index < controls.size(); index += 3)
        {
            auto row = area.removeFromTop(area.getHeight() / static_cast<int>(rows - index / 3));
            const int width = row.getWidth() / 3;
            for (size_t column = index; column < ju::jmin(index + 3, controls.size()); ++column)
                controls[column]->setBounds(row.removeFromLeft(width).reduced(3, 1));
        }
    }

private:
    friend class MainGui;

    struct Target { te::EditItemID track, plugin; };

    enum class Display { native, percent, ratio, toggle };

    struct Definition
    {
        const char* id;
        const ju::Identifier& property;
        Display display = Display::native;
    };

    static const std::vector<Definition>* parameterDefinitions(te::Plugin& plugin)
    {
        static const std::vector<Definition> reverb {
            { "room size", te::IDs::roomSize, Display::percent }, { "damping", te::IDs::damp, Display::percent },
            { "width", te::IDs::width, Display::percent }, { "wet level", te::IDs::wet },
            { "dry level", te::IDs::dry }, { "mode", te::IDs::mode, Display::toggle }
        };
        static const std::vector<Definition> compressor {
            { "threshold", te::IDs::threshold }, { "ratio", te::IDs::ratio, Display::ratio },
            { "attack", te::IDs::attack }, { "release", te::IDs::release }, { "output gain", te::IDs::outputDb }
        };
        if (dynamic_cast<te::ReverbPlugin*>(&plugin))
            return &reverb;
        if (dynamic_cast<te::CompressorPlugin*>(&plugin))
            return &compressor;
        return nullptr;
    }

    struct Control final : ju::Component
    {
        Control(Definition descriptor, te::AutomatableParameter& parameter)
            : definition(descriptor), range(parameter.getValueRange())
        {
            name.setText(parameter.getParameterName(), ju::dontSendNotification);
            name.setFont(ju::FontOptions(16.0f, ju::Font::bold));
            value.setJustificationType(ju::Justification::centredRight);
            maximum.setJustificationType(ju::Justification::centredRight);
            minimum.setFont(ju::FontOptions(13.0f));
            maximum.setFont(ju::FontOptions(13.0f));
            minimum.setText(format(parameter, mapRatio(range.getStart())), ju::dontSendNotification);
            maximum.setText(format(parameter, mapRatio(range.getEnd())), ju::dontSendNotification);
            for (auto* label : { &name, &value, &minimum, &maximum })
            {
                label->setColour(ju::Label::textColourId, ju::Colours::white);
                addAndMakeVisible(*label);
            }
            addAndMakeVisible(slider);
            slider.setTitle(name.getText());
            slider.setSliderStyle(ju::Slider::LinearHorizontal);
            slider.setTextBoxStyle(ju::Slider::NoTextBox, false, 0, 0);
            slider.setRange(range.getStart(), range.getEnd(), definition.display == Display::toggle ? 1.0 : 0.0);
            if (ju::String(definition.id) == "threshold")
                slider.setSkewFactorFromMidPoint(0.1);
            slider.setScrollWheelEnabled(false);
            slider.setColour(ju::Slider::backgroundColourId, ju::Colours::darkgrey);
            slider.setColour(ju::Slider::trackColourId, ju::Colours::cyan);
            slider.setColour(ju::Slider::thumbColourId, ju::Colours::lightgrey);
        }

        float mapRatio(float native) const
        {
            // The Compressor stores inverse ratio. Increasing the slider increases compression.
            return definition.display == Display::ratio ? range.getEnd() - native + range.getStart() : native;
        }

        ju::String format(te::AutomatableParameter& parameter, float native) const
        {
            if (definition.display == Display::percent)
                return ju::String(native * 100.0f, 1) + "%";
            return parameter.valueToString(native);
        }

        void update(te::AutomatableParameter& parameter)
        {
            const auto base = parameter.getCurrentExplicitValue();
            value.setText(format(parameter, base), ju::dontSendNotification);
            if (slider.getThumbBeingDragged() < 0)
                slider.setValue(mapRatio(base), ju::dontSendNotification);
        }

        void paint(ju::Graphics& graphics) override
        {
            graphics.fillAll(slider.isMouseButtonDown() || slider.hasKeyboardFocus(false)
                ? ju::Colour(0xff484848) : ju::Colour(0xff252525));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(5, 0);
            auto labels = area.removeFromTop(20);
            value.setBounds(labels.removeFromRight(ju::jmin(78, labels.getWidth() / 2)));
            name.setBounds(labels);
            auto endpoints = area.removeFromBottom(15);
            minimum.setBounds(endpoints.removeFromLeft(endpoints.getWidth() / 2));
            maximum.setBounds(endpoints);
            slider.setBounds(area);
        }

        Definition definition;
        ju::Range<float> range;
        ju::Label name, value, minimum, maximum;
        ju::Slider slider;
    };

    std::pair<te::AudioTrack*, te::Plugin*> resolve(Target ids) const
    {
        auto* project = edit();
        auto* track = project && ids.track.isValid() && ids.plugin.isValid()
            ? te::findAudioTrackForID(*project, ids.track) : nullptr;
        if (track)
            for (auto* plugin : track->pluginList.getPlugins())
                if (plugin->itemID == ids.plugin)
                    return { track, plugin };
        return {};
    }

    te::AutomatableParameter::Ptr parameterFor(const Control& control) const
    {
        if (target)
            if (auto* plugin = resolve(*target).second)
                return plugin->getAutomatableParameterByID(control.definition.id);
        return {};
    }

    std::function<te::Edit*()> edit;
    std::optional<Target> target;
    std::vector<std::unique_ptr<Control>> controls;
    ju::Label title, bypass, mixGuide { {}, "Mute Wet for full dry   |   Mute Dry for full wet" };
    ju::TextButton closeButton { "CLOSE" };
    bool highlighted = false;
};
