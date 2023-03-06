# What is this aap-juce-simple-host for?

It is a dogfooding project for [aap-juce](https://github.com/atsushieno/aap-juce) hosting.

We have JUCE AudioPluginHost port (aap-juce-plugin-host), but it is a
simple and stupid port of desktop application which is useless on mobile
in practice (you'll have hard time to connect those audio channels).

This app also makes diagnosing issues way simpler. It is based on CMake
so that no awkward project file generation occurs.

The project structure immitates what typical JUCE application ports in
aap-juce official ports form i.e. it puts the desktop JUCE project under
`external/AndroidPluginHost` (it is not really external).

## Licenses

aap-juce-simple-host is released under the GPLv3 license as JUCE requires.

