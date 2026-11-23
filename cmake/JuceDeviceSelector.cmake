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
