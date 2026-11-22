// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <JuceHeader.h>
#include <array>
#include <cstdio>
#include <numeric>
#include <set>
#include <thread>

namespace te = tracktion;
namespace ju = juce;

bool runEffectsFoundationTest(te::Engine&);

namespace
{
constexpr int maxTracks = 24;
constexpr const char* applicationTitle = "SimpleDAW v" SIMPLEDAW_RELEASE_VERSION;

class SimpleDawUIBehaviour final : public te::UIBehaviour
{
public:
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& job) override
    {
        while (job.runJob() == ju::ThreadPoolJob::jobNeedsRunningAgain)
        {
        }
    }
};

class SmokeTestEngineBehaviour final : public te::EngineBehaviour
{
public:
    bool autoInitialiseDeviceManager() override { return false; }
};

bool insertAudio(te::AudioTrack& track, const ju::File& file, te::TimePosition start = {})
{
    te::AudioFile audio(track.edit.engine, file);
    if (!audio.isValid() || audio.getLength() <= 0.0)
        return false;

    return track.insertWaveClip(file.getFileNameWithoutExtension(), file,
        {{ start, start + te::TimeDuration::fromSeconds(audio.getLength()) }, {}}, false) != nullptr;
}

template<typename PluginType>
bool insertBuiltIn(te::AudioTrack& track, bool synth = false)
{
    auto plugin = track.edit.getPluginCache().createNewPlugin(PluginType::xmlTypeName,
        te::PluginManager::createBuiltInPluginDescription<PluginType>(synth));
    if (plugin == nullptr)
        return false;

    track.pluginList.insertPlugin(plugin, 0, nullptr);
    return true;
}

bool exportWave(te::Edit& edit, const ju::File& destination)
{
    if (edit.getLength().inSeconds() <= 0.0)
        return false;

    ju::TemporaryFile output(destination);
    ju::WavAudioFormat wave;
    const te::Edit::ScopedRenderStatus rendering(edit, true);
    te::Renderer::Parameters options(edit);
    options.destFile = output.getFile();
    options.audioFormat = &wave;
    options.time = {{}, edit.getLength()};
    options.tracksToDo = te::toBitSet(te::getAllTracks(edit));
    options.canRenderInMono = false;
    options.useMasterPlugins = true;
    options.bitDepth = 24;
    return te::Renderer::renderToFile("Export stereo WAV", options) == output.getFile()
        && output.overwriteTargetFileWithTemporary();
}

bool writeTestWave(const ju::File& file)
{
    ju::WavAudioFormat wave;
    std::unique_ptr<ju::OutputStream> stream = file.createOutputStream();
    if (!stream)
        return false;
    std::unique_ptr<ju::AudioFormatWriter> writer(
        wave.createWriterFor(stream.get(), 44100.0, 1, 16, {}, 0));
    if (!writer)
        return false;
    stream.release();
    ju::AudioBuffer<float> samples(1, 4410);
    for (int i = 0; i < samples.getNumSamples(); ++i)
        samples.setSample(0, i, 0.15f * std::sin(ju::MathConstants<float>::twoPi * i / 100.0f));
    return writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples());
}

bool smokeTest(te::Engine& engine)
{
    auto folder = ju::File::getSpecialLocation(ju::File::tempDirectory)
        .getNonexistentChildFile("SimpleDAW-test", {});
    if (folder.createDirectory().failed())
        return false;

    const ju::ScopeGuard cleanup{[&] { folder.deleteRecursively(); }};
    auto input = folder.getChildFile("input.wav");
    if (!writeTestWave(input))
        return false;

    auto path = folder.getChildFile("test.tracktionedit");
    auto edit = te::createEmptyEdit(engine, path);
    edit->ensureNumberOfAudioTracks(2);
    const auto tracks = te::getAudioTracks(*edit);
    if (tracks.size() != 2 || !insertAudio(*tracks[0], input) || !insertAudio(*tracks[1], input)
        || !insertBuiltIn<te::ReverbPlugin>(*tracks[0])
        || !te::EditFileOperations(*edit).save(false, true, false))
        return false;

    edit.reset();
    edit = te::loadEditFromFile(engine, path);
    const auto reopened = te::getAudioTracks(*edit);
    if (reopened.size() != 2 || reopened[0]->getClips().size() != 1 || reopened[1]->getClips().size() != 1)
        return false;

    bool reverbReopened = false;
    for (auto* plugin : reopened[0]->pluginList.getPlugins())
        reverbReopened |= plugin->getPluginType() == te::ReverbPlugin::xmlTypeName;
    if (!reverbReopened)
        return false;

    auto output = folder.getChildFile("mix.wav");
    if (!exportWave(*edit, output))
        return false;

    ju::WavAudioFormat wave;
    std::unique_ptr<ju::AudioFormatReader> reader(wave.createReaderFor(output.createInputStream().release(), true));
    if (!reader || reader->numChannels != 2 || reader->sampleRate != 44100.0
        || reader->bitsPerSample != 24 || reader->lengthInSamples < 4410)
        return false;

    ju::AudioBuffer<float> samples(2, 4410);
    return reader->read(&samples, 0, samples.getNumSamples(), 0, true, true)
        && samples.getMagnitude(0, samples.getNumSamples()) > 0.01f;
}

class MainGui final : public ju::Component, private ju::Timer, private ju::MenuBarModel,
                      private ju::ApplicationCommandTarget
{
    #include "MainGui.inc"

private:
    using FileAction = std::function<void(const ju::File&)>;

    void report(const ju::String& text)
    {
        status.setText(text, ju::dontSendNotification);
    }

    bool ready()
    {
        if (!edit)
            report("Create or open a project first");
        else if (edit->getTransport().isRecording())
            report("Stop recording first");
        else if (edit->getTransport().isPlaying())
            report("Stop playback first");
        else
            return true;
        return false;
    }

    void chooseFile(const ju::String& title, const ju::String& filter, int flags, FileAction action)
    {
        chooser = std::make_unique<ju::FileChooser>(title,
            ju::File::getSpecialLocation(ju::File::userDocumentsDirectory), filter);
        chooser->launchAsync(flags, [action = std::move(action)](const ju::FileChooser& dialog) {
            if (auto file = dialog.getResult(); file != ju::File{})
                action(file);
        });
    }

    void chooseProject(bool create)
    {
        if (edit && !ready())
            return;

        chooseFile(create ? "Create project" : "Open project", "*.tracktionedit",
            ju::FileBrowserComponent::canSelectFiles
                | (create ? ju::FileBrowserComponent::saveMode : ju::FileBrowserComponent::openMode),
            [this, create](const ju::File& choice) {
                const auto file = create ? choice.withFileExtension("tracktionedit") : choice;
                if (create && file.exists())
                    return report("Choose a new project filename");
                if (!create)
                {
                    const auto xml = ju::parseXML(file);
                    if (!xml || !xml->hasTagName("EDIT"))
                        return report("Could not read this project");
                }
                if (!confirmProjectChanges())
                    return;

                auto next = create ? te::createEmptyEdit(engine, file) : te::loadEditFromFile(engine, file);
                if (!next)
                    return report("Could not open project");
                if (create)
                {
                    next->ensureNumberOfAudioTracks(1);
                    if (!te::EditFileOperations(*next).save(false, true, false))
                        return report("Could not create project file");
                }
                setProject(std::move(next), file);
                report("Project ready");
                warnUnavailablePlugins();
            });
    }

    bool hasProjectChanges()
    {
        edit->getUndoManager().dispatchPendingMessages();
        return edit->hasChangedSinceSaved();
    }

    bool confirmProjectChanges()
    {
        if (!edit)
            return true;
        if (!ready())
            return false;

        if (!hasProjectChanges())
            return true;

        const int choice = ju::AlertWindow::showYesNoCancelBox(ju::MessageBoxIconType::QuestionIcon,
            "Unsaved changes", "Save changes to " + projectFile.getFileName() + "?",
            "Save", "Don't Save", "Cancel", this);
        return choice == 2 || (choice == 1 && save());
    }

    te::AudioTrack* selectedTrack()
    {
        if (!ready())
            return nullptr;
        auto* track = trackAt(selection);
        if (!track)
            report("Select a track first");
        return track;
    }

    void configureDevices()
    {
        if (edit && !ready())
            return;

        ju::DialogWindow::LaunchOptions dialog;
        dialog.dialogTitle = "Audio and MIDI devices";
        dialog.dialogBackgroundColour = ju::Colours::darkgrey;
        dialog.content.setOwned(new ju::AudioDeviceSelectorComponent(engine.getDeviceManager().deviceManager,
            0, 32, 2, 32, true, false, true, false));
        dialog.content->setSize(480, 560);
        dialog.runModal();
    }

    void armInput(bool midi)
    {
        if (!edit || edit->getTransport().isRecording())
            return;
        const bool playing = edit->getTransport().isPlaying();
        if (midi && playing)
            return;
        auto* track = trackAt(selection);
        if (!track)
            return;

        if (const auto error = placementError(midi ? TrackType::midi : TrackType::audio, *track); error.isNotEmpty())
            return warnPlacement(error);

        auto& devices = engine.getDeviceManager();
        if (midi)
        {
            for (const auto& input : devices.getMidiInDevices())
                if (input->getDeviceType() == te::InputDevice::physicalMidiDevice)
                    input->setEnabled(true);
        }
        else if (!playing)
        {
            for (int i = 0; i < devices.getNumWaveInDevices(); ++i)
                if (auto* input = devices.getWaveInDevice(i))
                    input->setEnabled(true);
        }

        edit->getTransport().ensureContextAllocated();
        ju::PopupMenu menu;
        ju::StringArray choices;
        const auto trackID = track->itemID;
        const auto type = midi ? te::InputDevice::physicalMidiDevice : te::InputDevice::waveDevice;
        for (auto* instance : edit->getAllInputDevices())
        {
            auto& device = instance->getInputDevice();
            if (device.getDeviceType() == type && device.isEnabled())
            {
                choices.add(device.getDeviceID());
                const bool armed = isArmed(*instance, trackID);
                menu.addItem(choices.size(), device.getName(), !playing || armed, armed);
            }
        }

        if (choices.isEmpty())
            return report("No input available. Check Devices and try again");

        const int choice = menu.show();
        if (choice == 0)
            return;

        if (!edit || edit->getTransport().isRecording())
            return;
        track = te::findAudioTrackForID(*edit, trackID);
        if (!track)
            return;

        for (auto* input : edit->getAllInputDevices())
            if (input->getInputDevice().getDeviceType() == type
                && input->getInputDevice().getDeviceID() == choices[choice - 1])
            {
                if (edit->getTransport().isPlaying() && !isArmed(*input, trackID))
                    return report("Stop playback before assigning a new input");
                const auto inputName = input->getInputDevice().getName();
                if (const auto result = toggleInput(*input, *track); result.failed())
                    return report(result.getErrorMessage());
                report(inputName + (isArmed(*input, trackID) ? " armed on " : " removed from ") + track->getName());
                updateControls();
                return;
            }
        report("The selected input is no longer available");
    }

    void showMonitoringReport()
    {
        auto& manager = engine.getDeviceManager().deviceManager;
        auto* device = manager.getCurrentAudioDevice();
        if (!device)
            return report("Open Audio and MIDI Devices to select an interface first");

        const auto rate = device->getCurrentSampleRate();
        if (rate <= 0.0)
            return report("The selected audio device is inactive");
        const auto samples = [rate](int n) {
            return ju::String(n) + " samples (" + ju::String(n * 1000.0 / rate, 2) + " ms)";
        };
        const auto setup = manager.getAudioDeviceSetup();
        auto detail = manager.getCurrentAudioDeviceType()
            + "\nInput: " + setup.inputDeviceName + " / " + ju::String(device->getActiveInputChannels().countNumberOfSetBits()) + " ch"
            + "\nOutput: " + setup.outputDeviceName + " / " + ju::String(device->getActiveOutputChannels().countNumberOfSetBits()) + " ch"
            + "\nSample rate: " + ju::String(rate, 0) + " Hz"
            + "\nProcessing buffer: " + samples(device->getCurrentBufferSizeSamples())
            + "\nDriver input delay: " + samples(device->getInputLatencyInSamples())
            + "\nDriver output delay: " + samples(device->getOutputLatencyInSamples());

        if (edit)
        {
            if (auto* context = edit->getTransport().getCurrentPlaybackContext())
                detail += "\nTracktion graph latency: " + samples(context->getLatencySamples());
            if (auto* track = trackAt(selection))
            {
                detail += "\n\nSelected track: " + track->getName();
                bool routed = false;
                for (auto* input : edit->getAllInputDevices())
                    if (input->getInputDevice().getDeviceType() == te::InputDevice::waveDevice
                        && input->getTargets().contains(track->itemID))
                    {
                        routed = true;
                        const auto& source = input->getInputDevice();
                        using Mode = te::InputDevice::MonitorMode;
                        const auto mode = source.getMonitorMode();
                        const auto* label = mode == Mode::automatic ? "Auto" : mode == Mode::on ? "On" : "Off";
                        detail += "\n" + source.getName() + " - " + (isArmed(*input, track->itemID) ? "armed" : "disarmed")
                            + ", monitor " + label + (input->isLivePlayEnabled(*track) ? " (audible)" : " (inactive)");
                    }
                if (!routed)
                    detail += "\nNo physical audio input routed to this track.";
            }

            for (auto* plugin : te::getAllPlugins(*edit, true))
                if (plugin->isEnabled() && plugin->getLatencySeconds() > 0.0)
                {
                    auto* owner = plugin->getOwnerTrack();
                    detail += "\n" + (owner ? owner->getName() : ju::String("Master")) + " / " + plugin->getName()
                        + ": " + ju::String(plugin->getLatencySeconds() * 1000.0, 2) + " ms";
                }
        }
        detail += "\n\nReported delays are component values. Measuring guitar-to-speaker delay requires the interface.";
        ju::AlertWindow::showMessageBoxAsync(ju::MessageBoxIconType::InfoIcon, "Audio Monitoring Report", detail);
    }

    void record()
    {
        if (!ready())
            return;

        if (edit->recordingPunchInOut && !timeSelection)
            return report("Drag across the ruler to select the punch range");
        if (edit->recordingPunchInOut)
            edit->getTransport().setLoopRange(*timeSelection);
        if (paused)
            editCursor = edit->getTransport().getPosition();

        // Reopening an edit can restore arming before the first playback context exists.
        edit->getTransport().ensureContextAllocated();
        bool armed = false;
        for (auto* instance : edit->getAllInputDevices())
            for (auto* track : te::getAudioTracks(*edit))
                if (isArmed(*instance, track->itemID))
                {
                    const auto deviceType = instance->getInputDevice().getDeviceType();
                    const auto type = deviceType == te::InputDevice::waveDevice || deviceType == te::InputDevice::trackWaveDevice
                        ? TrackType::audio : TrackType::midi;
                    if (const auto error = placementError(type, *track); error.isNotEmpty())
                        return warnPlacement(error);
                    armed = true;
                }
        if (armed)
        {
            edit->getTransport().record(false);
            report(edit->getTransport().isRecording() ? "Recording" : "Could not start recording");
            return;
        }

        report("Arm an audio or MIDI input first");
    }

    void stop()
    {
        if (!edit)
            return;

        const bool recording = edit->getTransport().isRecording();
        edit->getTransport().stop(false, false);
        edit->getTransport().setPosition(editCursor);
        paused = false;
        if (recording)
        {
            edit->markAsChanged();
            report("Recording stopped. Save the project to keep the new items.");
        }
    }

    bool save()
    {
        if (!ready())
            return false;

        const bool saved = te::EditFileOperations(*edit).save(false, true, false);
        report(saved ? "Project saved" : "Could not save project");
        return saved;
    }

    void importWave()
    {
        auto* destination = selectedTrack();
        if (!destination)
            return;
        if (const auto error = placementError(TrackType::audio, *destination); error.isNotEmpty())
            return warnPlacement(error);

        const auto trackID = destination->itemID;
        auto* project = edit.get();
        const auto start = te::TimePosition::fromSeconds(snapSeconds(editCursor.inSeconds()));
        chooseFile("Import WAV at the edit cursor", "*.wav",
            ju::FileBrowserComponent::openMode | ju::FileBrowserComponent::canSelectFiles,
            [this, project, trackID, start](const ju::File& file) {
                if (edit.get() == project && ready())
                    if (auto* track = te::findAudioTrackForID(*edit, trackID))
                    {
                        if (const auto error = placementError(TrackType::audio, *track); error.isNotEmpty())
                            return warnPlacement(error);
                        edit->getUndoManager().beginNewTransaction("Import audio");
                        report(insertAudio(*track, file, start) ? "WAV imported" : "Could not import WAV");
                    }
            });
    }

    void loadVst3()
    {
        if (!selectedTrack())
            return;

        chooseFile("Select an installed VST3", "*.vst3",
            ju::FileBrowserComponent::openMode | ju::FileBrowserComponent::canSelectFiles
                | ju::FileBrowserComponent::canSelectDirectories,
            [this](const ju::File& file) {
                auto* track = selectedTrack();
                if (!track || !file.hasFileExtension("vst3"))
                    return;

                auto& manager = engine.getPluginManager();
                ju::OwnedArray<ju::PluginDescription> descriptions;
                for (auto* format : manager.pluginFormatManager.getFormats())
                    if (format->getName() == "VST3")
                        manager.knownPluginList.scanAndAddFile(file.getFullPathName(), false, descriptions, *format);

                if (descriptions.isEmpty())
                    return report("No VST3 found at this path");

                ju::PopupMenu menu;
                for (int i = 0; i < descriptions.size(); ++i)
                    menu.addItem(i + 1, descriptions[i]->name);

                if (const int choice = menu.show(); choice > 0)
                {
                    auto plugin = edit->getPluginCache().createNewPlugin(
                        te::ExternalPlugin::xmlTypeName, *descriptions[choice - 1]);
                    if (plugin == nullptr || plugin->getWrappedAudioProcessor() == nullptr)
                        return report("Could not load VST3");

                    track->pluginList.insertPlugin(plugin, 0, nullptr);
                    report("Inserted " + descriptions[choice - 1]->name);
                }
            });
    }

    void exportProject()
    {
        if (!ready())
            return;

        chooseFile("Export stereo WAV", "*.wav",
            ju::FileBrowserComponent::saveMode | ju::FileBrowserComponent::canSelectFiles,
            [this](const ju::File& file) {
                if (file.withFileExtension("wav").exists())
                    return report("Choose a new WAV filename");
                if (ready())
                    report(exportWave(*edit, file.withFileExtension("wav")) ? "WAV exported" : "WAV export failed");
            });
    }

    std::unique_ptr<ju::FileChooser> chooser;
};

class Application final : public ju::JUCEApplication
{
public:
    const ju::String getApplicationName() override { return "SimpleDAW"; }
    const ju::String getApplicationVersion() override { return ProjectInfo::versionString; }

    void initialise(const ju::String& command) override
    {
        if (te::PluginManager::startChildProcessPluginScan(command))
            return;

        const bool testing = command == "--smoke-test" || command == "--gui-test"
            || command == "--correction-test" || command == "--uat-test"
            || command == "--effects-foundation-test";
        std::unique_ptr<te::EngineBehaviour> engineBehaviour;
        if (testing)
            engineBehaviour = std::make_unique<SmokeTestEngineBehaviour>();
        engine = std::make_unique<te::Engine>(testing ? "SimpleDAWSmokeTest" : "SimpleDAW",
            std::make_unique<SimpleDawUIBehaviour>(), std::move(engineBehaviour));
        if (testing)
        {
            bool passed = false;
            if (command == "--gui-test")
                passed = MainGui::test(*engine);
            else if (command == "--correction-test")
                passed = MainGui::testCorrections(*engine);
            else if (command == "--uat-test")
                passed = MainGui::testUniversalActions(*engine);
            else if (command == "--effects-foundation-test")
                passed = runEffectsFoundationTest(*engine);
            else
                passed = smokeTest(*engine);
            setApplicationReturnValue(passed ? 0 : 1);
            quit();
            return;
        }

        window = std::make_unique<Window>();
        window->setUsingNativeTitleBar(true);
        auto* gui = new MainGui(*engine);
        window->setContentOwned(gui, true);
        gui->installWindowShortcuts(*window);
        window->setResizable(true, false);
        window->setResizeLimits(1060, 580, 7680, 4320);
        auto bounds = window->getBounds();
        if (const auto* display = ju::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            bounds = bounds.withSize(ju::jmax(1060, ju::jmin(bounds.getWidth(), display->userArea.getWidth() - 16)),
                                     ju::jmax(580, ju::jmin(bounds.getHeight(), display->userArea.getHeight() - 32)));
        window->centreWithSize(bounds.getWidth(), bounds.getHeight());
        window->setVisible(true);
    }

    void systemRequestedQuit() override
    {
        if (window)
            if (auto* gui = dynamic_cast<MainGui*>(window->getContentComponent()); gui && !gui->prepareToQuit())
                return;
        quit();
    }

    void shutdown() override
    {
        window.reset();
        engine.reset();
    }

private:
    struct Window final : ju::DocumentWindow
    {
        Window() : DocumentWindow(applicationTitle, ju::Colours::black, allButtons) {}
        void closeButtonPressed() override { ju::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(Application)
