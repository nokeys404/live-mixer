import React from 'react';
import { AudioSettingsView } from './components/AudioSettingsView';
import { ArchitectureTestViewer } from './components/ArchitectureTestViewer';

export default function App() {
  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100 flex flex-col justify-start items-center p-4 sm:p-8 space-y-6 antialiased selection:bg-emerald-600 selection:text-white">
      {/* App Header Banner */}
      <div className="w-full max-w-2xl flex items-center justify-between px-1 text-xs text-zinc-400">
        <div className="flex items-center space-x-2">
          <span className="w-2 h-2 rounded-full bg-emerald-500"></span>
          <span className="font-mono font-medium text-zinc-300">
            Live Mixer V0.1 • Windows Desktop C++ / JUCE Foundation
          </span>
        </div>
        <div className="font-mono text-zinc-500">
          Target: x64 Windows / CMake
        </div>
      </div>

      {/* Main Live Mixer Settings Window */}
      <AudioSettingsView />

      {/* C++ JUCE Architecture & Automated Test Runner */}
      <div className="w-full max-w-2xl">
        <ArchitectureTestViewer />
      </div>
    </div>
  );
}
