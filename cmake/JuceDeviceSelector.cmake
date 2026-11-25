# Apply the selector fixes to the pinned JUCE source, including offline source builds.
set(selector "${juce_SOURCE_DIR}/modules/juce_audio_utils/gui/juce_AudioDeviceSelectorComponent.cpp")
file(READ "${selector}" selector_source)
string(REPLACE "\r\n" "\n" selector_source "${selector_source}")
if(NOT selector_source MATCHES "SimpleDAW selector refresh")
    function(replace_selector_anchor before after)
        string(FIND "${selector_source}" "${before}" anchor)
        if(anchor EQUAL -1)
            message(FATAL_ERROR "Device selector refresh requires the pinned JUCE selector")
        endif()
        string(REPLACE "${before}" "${after}" updated "${selector_source}")
        set(selector_source "${updated}" PARENT_SCOPE)
    endfunction()

    replace_selector_anchor([=[            deviceManager.setMidiInputDeviceEnabled (identifier, ! deviceManager.isMidiInputDeviceEnabled (identifier));]=] [=[
            deviceManager.setMidiInputDeviceEnabled (identifier, ! deviceManager.isMidiInputDeviceEnabled (identifier));
            repaintRow (row); // SimpleDAW selector refresh
]=])
    replace_selector_anchor([=[        deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);]=] [=[
        type->scanForDevices();
        deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);
]=])
    replace_selector_anchor([=[            updateSelectedInput();
            updateSelectedOutput();
            updateControlPanelButton();
            resized();
]=] "")
    replace_selector_anchor([=[        if (error.isNotEmpty())
            messageBox = AlertWindow::showScopedAsync]=] [=[
        updateAllControls();

        if (error.isNotEmpty())
            messageBox = AlertWindow::showScopedAsync]=])
    replace_selector_anchor([=[                setup.manager->setAudioDeviceSetup (config, true);
            }
        }

        static void flipBit]=] [=[
                setup.manager->setAudioDeviceSetup (config, true);
                for (int channel = 0; channel < items.size(); ++channel)
                    repaintRow (channel);
            }
        }

        static void flipBit]=])
    file(WRITE "${selector}" "${selector_source}")
endif()

# Rebuild controls after the current ComboBox callback has unwound. In
# particular, a rate/buffer refresh must not clear or replace its own callback.
if(NOT selector_source MATCHES "SimpleDAW deferred device refresh")
    function(replace_device_refresh_anchor before after)
        string(FIND "${selector_source}" "${before}" anchor)
        if(anchor EQUAL -1)
            message(FATAL_ERROR "Deferred device refresh requires the pinned JUCE selector")
        endif()
        string(REPLACE "${before}" "${after}" updated "${selector_source}")
        set(selector_source "${updated}" PARENT_SCOPE)
    endfunction()

    replace_device_refresh_anchor([=[class AudioDeviceSettingsPanel : public Component,
                                 private ChangeListener]=] [=[
class AudioDeviceSettingsPanel : public Component,
                                 private ChangeListener,
                                 private AsyncUpdater // SimpleDAW deferred device refresh]=])
    replace_device_refresh_anchor([=[    ~AudioDeviceSettingsPanel() override
    {
        setup.manager->removeChangeListener (this);]=] [=[    ~AudioDeviceSettingsPanel() override
    {
        cancelPendingUpdate();
        setup.manager->removeChangeListener (this);]=])
    replace_device_refresh_anchor([=[        updateAllControls();

        if (error.isNotEmpty())
            messageBox = AlertWindow::showScopedAsync]=] [=[        triggerAsyncUpdate();

        if (error.isNotEmpty())
            messageBox = AlertWindow::showScopedAsync]=])
    replace_device_refresh_anchor([=[    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updateAllControls();
    }]=] [=[    void changeListenerCallback (ChangeBroadcaster*) override
    {
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        updateAllControls();
    }]=])
    foreach(dropdown sampleRateDropDown bufferSizeDropDown)
        replace_device_refresh_anchor("            ${dropdown}->clear();\n            ${dropdown}->onChange = nullptr;"
                                      "            ${dropdown}->clear (dontSendNotification);")
    endforeach()
    file(WRITE "${selector}" "${selector_source}")
endif()
