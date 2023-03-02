#include <iostream>
#include <memory>
#include <JuceHeader.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#if JUCEAAP_ENABLED
#include <juceaap_audio_processors/juceaap_audio_plugin_format.h>
#endif

const char* APPLICATION_NAME = "AndroidPluginHost";
const char* SETTINGS_PLUGIN_LIST = "plugin-list";

class AppModel {
    ApplicationProperties settings{};
    std::unique_ptr<AudioDeviceManager> audioDeviceManager{nullptr};
    KnownPluginList knownPluginList{};
    AudioPluginFormatManager pluginFormatManager{};
    PluginDescription selectedPlugin{};
#if JUCEAAP_ENABLED
    std::unique_ptr<juceaap::AndroidAudioPluginFormat> androidAudioPluginFormat{nullptr};
#endif

public:
    std::unique_ptr<AudioPluginInstance> instance{nullptr};

    AppModel() {
        PropertiesFile::Options options{};
        options.applicationName = APPLICATION_NAME;
        options.storageFormat = PropertiesFile::StorageFormat::storeAsXML;
        settings.setStorageParameters(options);
        auto pluginList = settings.getUserSettings()->getXmlValue(SETTINGS_PLUGIN_LIST);

        if (pluginList)
            knownPluginList.recreateFromXml(*pluginList);

#if JUCEAAP_ENABLED
        androidAudioPluginFormat = std::make_unique<juceaap::AndroidAudioPluginFormat>();
#endif
    }
    ~AppModel() = default;

    AudioPluginFormatManager& getPluginFormatManager() { return pluginFormatManager; }
    KnownPluginList& getKnownPluginList() { return knownPluginList; }

    void scanPlugins(AudioPluginFormat* format) {
        OwnedArray<PluginDescription> pluginDescriptions{};
        {
            auto& list = getKnownPluginList();
            auto searchPaths = format->getDefaultLocationsToSearch();
            auto pluginFiles = format->searchPathsForPlugins(searchPaths, true);
            for (auto& pluginFile : pluginFiles) {
                printf("FileOrIdentifier: %s :\n", pluginFile.toRawUTF8());
                try {
                    if (!list.getBlacklistedFiles().contains(pluginFile))
                        format->findAllTypesForFile(pluginDescriptions, pluginFile);
                } catch(std::runtime_error& ex) {
                    // swallow scanner failures
                    list.addToBlacklist(pluginFile);
                    saveKnownPluginList();
                }
            }
        }
        for (auto pluginDescription : pluginDescriptions)
            if (pluginDescription->pluginFormatName == format->getName())
                getKnownPluginList().addType(*pluginDescription);
        saveKnownPluginList();
    }

    void saveKnownPluginList() {
        settings.getUserSettings()->setValue(SETTINGS_PLUGIN_LIST, knownPluginList.createXml().get());
        settings.saveIfNeeded();
    }

    PluginDescription& getSelectedPlugin() { return selectedPlugin; }
    void setSelectedPlugin(PluginDescription plugin) {
        selectedPlugin = plugin;
    }

#if JUCEAAP_ENABLED
    juceaap::AndroidAudioPluginFormat* getAndroidAudioPluginFormat() {
        return androidAudioPluginFormat.get();
    }
#endif
};

std::unique_ptr<AppModel> _appModel{};
AppModel* getAppModel() {
    if (!_appModel)
        _appModel = std::make_unique<AppModel>();
    return _appModel.get();
}

#define appModel (getAppModel())

class MainComponent : public Component {
    TextButton buttonScanPlugins{"Scan plugins"};
    ComboBox comboBoxPluginFormats{};
    ComboBox comboBoxPluginVendors{};
    ComboBox comboBoxPlugins{};
    TextButton buttonInstantiate{"Instantiate"};
    Label labelStatusText{};

    ToggleButton mpeToggle{"MPE"};
    MidiKeyboardState keyboardState;
    MidiKeyboardComponent keyboard{keyboardState, KeyboardComponentBase::Orientation::horizontalKeyboard};
    MPEZoneLayout mpeZoneLayout{MPEZone{MPEZone::Type::lower, 7}, MPEZone{MPEZone::Type::upper, 7}};
    MPEInstrument mpeInstrument{mpeZoneLayout};
    MPEKeyboardComponent mpeKeyboard{mpeInstrument, KeyboardComponentBase::Orientation::horizontalKeyboard};

    TextButton buttonSetupMidiInDevices{"Setup MIDI In"};
    TextButton buttonSetupMidiOutDevices{"Setup MIDI Out"};
    ComboBox comboBoxMidiInDevices{};
    ComboBox comboBoxMidiOutDevices{};

    class AudioPluginFormatComparer {
    public:
        int compareElements (AudioPluginFormat* first, AudioPluginFormat* second)
        {
            return first->getName().compare(second->getName());
        }
    };

public:
    MainComponent() {
        auto& formatManager = appModel->getPluginFormatManager();
        formatManager.addDefaultFormats();
#if ANDROID
        formatManager.addFormat(appModel->getAndroidAudioPluginFormat());
#endif

        // trivial formats first, then non-trivial formats follow.
        juce::Array<AudioPluginFormat*> formats{formatManager.getFormats()};
        AudioPluginFormatComparer cmp{};
        formats.sort<AudioPluginFormatComparer>(cmp);
        for (auto format : formats)
            if (format->isTrivialToScan())
                comboBoxPluginFormats.addItem(format->getName(), comboBoxPluginFormats.getNumItems() + 1);
        for (auto format : formatManager.getFormats())
            if (!format->isTrivialToScan())
                comboBoxPluginFormats.addItem(format->getName(), comboBoxPluginFormats.getNumItems() + 1);
        comboBoxPluginFormats.setSelectedId(1);

        buttonScanPlugins.onClick = [this, formats] {
            appModel->scanPlugins(formats[comboBoxPluginFormats.getSelectedId() - 1]);
            updatePluginListOnUI();
        };

        comboBoxPluginVendors.onChange = [&] {
            bool filtered = comboBoxPluginVendors.getSelectedId() > 1;
            auto vendor = comboBoxPluginVendors.getText();
            Array<PluginDescription> plugins{};
            for (auto& desc : appModel->getKnownPluginList().getTypes())
                if (!filtered || desc.manufacturerName == vendor)
                    plugins.add(desc);
            comboBoxPlugins.clear(NotificationType::dontSendNotification);
            for (auto& desc : plugins)
                comboBoxPlugins.addItem(desc.descriptiveName, comboBoxPlugins.getNumItems() + 1);
        };

        comboBoxPlugins.onChange = [&] {
            bool filtered = comboBoxPluginVendors.getSelectedId() > 1;
            auto vendor = comboBoxPluginVendors.getText();
            for (auto& desc : appModel->getKnownPluginList().getTypes()) {
                if (!filtered || desc.manufacturerName == vendor) {
                    if (desc.descriptiveName == comboBoxPlugins.getText()) {
                        appModel->setSelectedPlugin(desc);
                        return;
                    }
                }
            }
        };

        buttonInstantiate.onClick = [&] {
            auto& desc = appModel->getSelectedPlugin();
            if (desc.uniqueId != 0) {
                for (auto format : formatManager.getFormats()) {
                    if (format->getName() != comboBoxPluginFormats.getText())
                        continue;
                    format->createPluginInstanceAsync(desc, 44100, 1024 * 32, [&](std::unique_ptr<AudioPluginInstance> instance, String error) {
                        if (error.isEmpty()) {
                            appModel->instance = std::move(instance);
                            appModel->instance->prepareToPlay(44100, 1024);
                            AudioBuffer<float> buffer{2, 1024};
                            MidiBuffer midiMessages{};
                            appModel->instance->processBlock(buffer, midiMessages);
                        } else {
                            AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Plugin Error", error);
                        }
                    });
                }
            }
        };

        // setup components
        comboBoxPluginFormats.setBounds(0, 0, 100, 50);
        buttonScanPlugins.setBounds(0, 50, 200, 50);
        comboBoxPluginVendors.setBounds(0, 100, 400, 50);
        comboBoxPlugins.setBounds(0, 150, 400, 50);
        buttonInstantiate.setBounds(0, 200, 200, 50);
        labelStatusText.setBounds(0, 250, 200, 50);

        addAndMakeVisible(comboBoxPluginFormats);
        addAndMakeVisible(buttonScanPlugins);
        addAndMakeVisible(comboBoxPluginVendors);
        addAndMakeVisible(comboBoxPlugins);
        addAndMakeVisible(buttonInstantiate);
        addAndMakeVisible(labelStatusText);
        labelStatusText.setText("Ready", NotificationType::sendNotificationAsync);

        updatePluginListOnUI();

        // Set MIDI/MPE keyboard
        mpeToggle.setBounds(0, 300, 400, 50);
        mpeToggle.onClick = [&] {
            if (keyboard.isVisible())
                mpeKeyboard.setLowestVisibleKey(keyboard.getLowestVisibleKey());
            else
                keyboard.setLowestVisibleKey(mpeKeyboard.getLowestVisibleKey());
            keyboard.setVisible(!keyboard.isVisible());
            mpeKeyboard.setVisible(!mpeKeyboard.isVisible());
        };
        addAndMakeVisible(mpeToggle);
        keyboard.setBounds(0, 350, 400, 50);
        addAndMakeVisible(keyboard);
        mpeInstrument.enableLegacyMode();
        mpeInstrument.setZoneLayout(mpeZoneLayout);
        mpeKeyboard.setBounds(0, 350, 400, 50);
        addAndMakeVisible(mpeKeyboard);
        mpeKeyboard.setVisible(false);

        /*
        // Setup MIDI devices playgound
        buttonSetupMidiInDevices.onClick = [&] {
            comboBoxMidiInDevices.clear(NotificationType::sendNotificationAsync);
            for (auto midiDevice : juce::MidiInput::getAvailableDevices())
                comboBoxMidiInDevices.addItem(midiDevice.name, comboBoxMidiInDevices.getNumItems() + 1);
        };
        buttonSetupMidiOutDevices.onClick = [&] {
            comboBoxMidiOutDevices.clear(NotificationType::sendNotificationAsync);
            auto defaultOut = juce::MidiOutput::getDefaultDevice();
            for (auto midiDevice : juce::MidiOutput::getAvailableDevices())
                comboBoxMidiOutDevices.addItem(midiDevice.name, comboBoxMidiOutDevices.getNumItems() + 1);
        };

        buttonSetupMidiInDevices.setBounds(0, 300, 150, 50);
        buttonSetupMidiOutDevices.setBounds(0, 350, 150, 50);
        comboBoxMidiInDevices.setBounds(200, 300, 200, 50);
        comboBoxMidiOutDevices.setBounds(200, 350, 200, 50);
        addAndMakeVisible(buttonSetupMidiInDevices);
        addAndMakeVisible(buttonSetupMidiOutDevices);
        addAndMakeVisible(comboBoxMidiInDevices);
        addAndMakeVisible(comboBoxMidiOutDevices);
        */
    }

    void updatePluginListOnUI() {
        comboBoxPlugins.clear(NotificationType::sendNotificationAsync);
        if (appModel->getKnownPluginList().getNumTypes() == 0)
            return;
        Array<String> vendors{};
        for (auto &desc: appModel->getKnownPluginList().getTypes()) {
            if (!vendors.contains(desc.manufacturerName))
                vendors.add(desc.manufacturerName);
            comboBoxPlugins.addItem(desc.descriptiveName, comboBoxPlugins.getNumItems() + 1);
        }
        comboBoxPluginVendors.clear(NotificationType::dontSendNotification);
        const char* allVendors = "--- All Vendors ---";
        comboBoxPluginVendors.addItem(allVendors, 1);
        for (auto &vendor : vendors)
            if (vendor.isNotEmpty()) // sometimes it is empty
                comboBoxPluginVendors.addItem(vendor, comboBoxPluginVendors.getNumItems() + 2);
        comboBoxPluginVendors.setSelectedId(1, NotificationType::sendNotificationAsync);
    }
};

class AndroidPluginHostApplication  : public JUCEApplication
{
public:
    //==============================================================================
    AndroidPluginHostApplication() = default;

    ~AndroidPluginHostApplication() override {
        _appModel.reset(nullptr);
    }

    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const String& commandLine) override
    {
        mainWindow = std::make_unique<MainAppWindow> (getApplicationName());


    }

    bool backButtonPressed() override    { return true; }
    void shutdown() override             { mainWindow = nullptr; }

    //==============================================================================
    void systemRequestedQuit() override                   { quit(); }
    void anotherInstanceStarted (const String&) override  {}

    ApplicationCommandManager& getGlobalCommandManager()  { return commandManager; }

private:
    class MainAppWindow    : public DocumentWindow
    {
    public:
        explicit MainAppWindow (const String& name)
                : DocumentWindow (name, Desktop::getInstance().getDefaultLookAndFeel()
                                          .findColour (ResizableWindow::backgroundColourId),
                                  DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            setResizeLimits (400, 400, 10000, 10000);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);

            auto& desktop = Desktop::getInstance();

            desktop.setOrientationsEnabled (Desktop::allOrientations);
            desktop.setKioskModeComponent (this);
#else
            setBounds ((int) (0.1f * (float) getParentWidth()),
                       (int) (0.1f * (float) getParentHeight()),
                       jmax (850, (int) (0.5f * (float) getParentWidth())),
                       jmax (600, (int) (0.7f * (float) getParentHeight())));
#endif

            setContentOwned (new MainComponent(), false);
            setVisible (true);
        }

        void closeButtonPressed() override    { JUCEApplication::getInstance()->systemRequestedQuit(); }

#if JUCE_IOS || JUCE_ANDROID
        void parentSizeChanged() override
        {
            getMainComponent().resized();
        }
#endif

        //==============================================================================
        MainComponent& getMainComponent()    { return *dynamic_cast<MainComponent*> (getContentComponent()); }

    private:
        std::unique_ptr<Component> taskbarIcon;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainAppWindow)
    };

    std::unique_ptr<MainAppWindow> mainWindow;
    ApplicationCommandManager commandManager;
};

ApplicationCommandManager& getGlobalCommandManager()
{
    return dynamic_cast<AndroidPluginHostApplication*> (JUCEApplication::getInstance())->getGlobalCommandManager();
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (AndroidPluginHostApplication)
