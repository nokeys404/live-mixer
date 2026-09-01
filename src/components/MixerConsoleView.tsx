import React, { useEffect, useState } from 'react';
import { globalAudioEngine } from '../audioEngineSim';
import { ChannelStripState, MasterStripState, AudioMetrics } from '../types';

export const MixerConsoleView: React.FC = () => {
  const [metrics, setMetrics] = useState<AudioMetrics>(globalAudioEngine.getMetrics());
  const [ch1, setCh1] = useState<ChannelStripState>(globalAudioEngine.getChannel1());
  const [ch2, setCh2] = useState<ChannelStripState>(globalAudioEngine.getChannel2());
  const [ch34, setCh34] = useState<ChannelStripState>(globalAudioEngine.getChannel34());
  const [master, setMaster] = useState<MasterStripState>(globalAudioEngine.getMaster());

  useEffect(() => {
    const timer = setInterval(() => {
      setMetrics(globalAudioEngine.getMetrics());
      setCh1(globalAudioEngine.getChannel1());
      setCh2(globalAudioEngine.getChannel2());
      setCh34(globalAudioEngine.getChannel34());
      setMaster(globalAudioEngine.getMaster());
    }, 33); // ~30 Hz UI refresh rate

    return () => clearInterval(timer);
  }, []);

  const handleStartStop = async () => {
    if (metrics.audioState === 'Running') {
      globalAudioEngine.stop();
    } else {
      await globalAudioEngine.start();
    }
  };

  const updateCh = (
    id: 1 | 2 | 3,
    param: 'gainDb' | 'panOrBalance' | 'faderDb' | 'muted' | 'solo',
    value: number | boolean
  ) => {
    globalAudioEngine.setChannelParam(id, param, value);
    if (id === 1) setCh1(globalAudioEngine.getChannel1());
    if (id === 2) setCh2(globalAudioEngine.getChannel2());
    if (id === 3) setCh34(globalAudioEngine.getChannel34());
  };

  const updateMaster = (param: 'faderDb' | 'muted', value: number | boolean) => {
    globalAudioEngine.setMasterParam(param, value);
    setMaster(globalAudioEngine.getMaster());
  };

  // Helper for meter bar height percentage
  const getMeterHeightPercent = (peakLin: number) => {
    const clamped = Math.min(1.0, Math.max(0, peakLin));
    return `${Math.round(clamped * 100)}%`;
  };

  const renderMeter = (peakL: number, peakR: number, isStereo: boolean, clipped: boolean) => {
    return (
      <div className="flex flex-col items-center h-full w-full bg-zinc-950 rounded border border-zinc-800 p-1">
        {/* Clip LED */}
        <div
          className={`w-full h-2 rounded-xs mb-1 transition-colors ${
            clipped ? 'bg-red-500 shadow-sm shadow-red-500/50' : 'bg-zinc-800'
          }`}
          title={clipped ? 'Clipping Detected' : 'No Clip'}
        />

        {/* Meter Bar Area */}
        <div className="flex-1 w-full flex space-x-1 items-end justify-center bg-zinc-900/80 rounded-xs p-0.5 overflow-hidden">
          {/* Left Bar */}
          <div className="flex-1 h-full flex flex-col justify-end bg-zinc-950 rounded-2xs overflow-hidden">
            <div
              className="w-full bg-gradient-to-t from-emerald-500 via-amber-400 to-red-500 transition-all duration-75"
              style={{ height: getMeterHeightPercent(peakL) }}
            />
          </div>

          {/* Right Bar if Stereo */}
          {isStereo && (
            <div className="flex-1 h-full flex flex-col justify-end bg-zinc-950 rounded-2xs overflow-hidden">
              <div
                className="w-full bg-gradient-to-t from-emerald-500 via-amber-400 to-red-500 transition-all duration-75"
                style={{ height: getMeterHeightPercent(peakR) }}
              />
            </div>
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="w-full max-w-4xl bg-zinc-900 border border-zinc-800 rounded-lg shadow-2xl p-4 flex flex-col space-y-4">
      {/* Header Bar */}
      <div className="flex items-center justify-between border-b border-zinc-800 pb-3">
        <div className="flex items-center space-x-3">
          <h2 className="text-base font-bold text-zinc-100 tracking-wide">
            LIVE MIXER CONSOLE
          </h2>
          <span className="text-xs text-zinc-500 font-mono">Stage 2 • 4-Channel</span>
        </div>

        <div className="flex items-center space-x-3">
          {/* Audio Engine Status Badge */}
          <div className="flex items-center space-x-2 px-2.5 py-1 bg-zinc-950 rounded border border-zinc-800">
            <span
              className={`w-2 h-2 rounded-full ${
                metrics.audioState === 'Running'
                  ? 'bg-emerald-500 animate-pulse'
                  : metrics.audioState === 'Error'
                  ? 'bg-red-500'
                  : 'bg-zinc-500'
              }`}
            />
            <span className="text-xs font-mono text-zinc-300">
              {metrics.audioState.toUpperCase()}
            </span>
          </div>

          {/* Start/Stop Audio Engine Button */}
          <button
            onClick={handleStartStop}
            className={`px-3 py-1 text-xs font-semibold rounded transition-colors ${
              metrics.audioState === 'Running'
                ? 'bg-red-950/80 text-red-300 border border-red-800 hover:bg-red-900'
                : 'bg-emerald-600 text-white hover:bg-emerald-500 shadow-sm'
            }`}
          >
            {metrics.audioState === 'Running' ? 'STOP ENGINE' : 'START ENGINE'}
          </button>
        </div>
      </div>

      {/* 4-Channel Mixer Grid */}
      <div className="grid grid-cols-4 gap-3 h-[420px]">
        {/* CH1 MIC (Mono) */}
        <div className="bg-zinc-950/70 border border-zinc-800/80 rounded-md p-3 flex flex-col justify-between">
          <div className="text-center border-b border-zinc-800 pb-2">
            <div className="text-xs font-bold text-zinc-200">CH1 MIC</div>
            <div className="text-[10px] text-zinc-500 font-mono">In 1 (Mono)</div>
          </div>

          {/* Gain Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">GAIN</label>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value={ch1.gainDb}
              onChange={(e) => updateCh(1, 'gainDb', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-emerald-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {ch1.gainDb > 0 ? `+${ch1.gainDb.toFixed(1)}` : ch1.gainDb.toFixed(1)} dB
            </span>
          </div>

          {/* Pan Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">PAN</label>
            <input
              type="range"
              min="-1"
              max="1"
              step="0.05"
              value={ch1.panOrBalance}
              onChange={(e) => updateCh(1, 'panOrBalance', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-blue-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {Math.abs(ch1.panOrBalance) < 0.05
                ? 'C'
                : ch1.panOrBalance < 0
                ? `L${Math.round(Math.abs(ch1.panOrBalance) * 100)}`
                : `R${Math.round(ch1.panOrBalance * 100)}`}
            </span>
          </div>

          {/* Meter & Fader Section */}
          <div className="flex-1 flex space-x-2 my-2 min-h-[140px]">
            <div className="w-4 h-full">
              {renderMeter(ch1.peakL, ch1.peakR, false, ch1.clipped)}
            </div>
            <div className="flex-1 flex flex-col items-center justify-between">
              <input
                type="range"
                min="-60"
                max="10"
                step="0.5"
                value={ch1.faderDb}
                onChange={(e) => updateCh(1, 'faderDb', parseFloat(e.target.value))}
                className="h-full -rotate-90 w-28 my-auto appearance-none bg-zinc-800 rounded accent-emerald-500 cursor-pointer"
              />
              <span className="text-[11px] font-mono font-bold text-zinc-300">
                {ch1.faderDb <= -60
                  ? '-INF'
                  : `${ch1.faderDb > 0 ? '+' : ''}${ch1.faderDb.toFixed(1)} dB`}
              </span>
            </div>
          </div>

          {/* Mute & Solo Buttons */}
          <div className="grid grid-cols-2 gap-1 pt-1 border-t border-zinc-800/80">
            <button
              onClick={() => updateCh(1, 'muted', !ch1.muted)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch1.muted
                  ? 'bg-red-600 text-white shadow-sm shadow-red-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              MUTE
            </button>
            <button
              onClick={() => updateCh(1, 'solo', !ch1.solo)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch1.solo
                  ? 'bg-amber-500 text-black shadow-sm shadow-amber-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              SOLO
            </button>
          </div>
        </div>

        {/* CH2 INST (Mono) */}
        <div className="bg-zinc-950/70 border border-zinc-800/80 rounded-md p-3 flex flex-col justify-between">
          <div className="text-center border-b border-zinc-800 pb-2">
            <div className="text-xs font-bold text-zinc-200">CH2 INST</div>
            <div className="text-[10px] text-zinc-500 font-mono">In 2 (Mono)</div>
          </div>

          {/* Gain Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">GAIN</label>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value={ch2.gainDb}
              onChange={(e) => updateCh(2, 'gainDb', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-emerald-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {ch2.gainDb > 0 ? `+${ch2.gainDb.toFixed(1)}` : ch2.gainDb.toFixed(1)} dB
            </span>
          </div>

          {/* Pan Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">PAN</label>
            <input
              type="range"
              min="-1"
              max="1"
              step="0.05"
              value={ch2.panOrBalance}
              onChange={(e) => updateCh(2, 'panOrBalance', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-blue-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {Math.abs(ch2.panOrBalance) < 0.05
                ? 'C'
                : ch2.panOrBalance < 0
                ? `L${Math.round(Math.abs(ch2.panOrBalance) * 100)}`
                : `R${Math.round(ch2.panOrBalance * 100)}`}
            </span>
          </div>

          {/* Meter & Fader Section */}
          <div className="flex-1 flex space-x-2 my-2 min-h-[140px]">
            <div className="w-4 h-full">
              {renderMeter(ch2.peakL, ch2.peakR, false, ch2.clipped)}
            </div>
            <div className="flex-1 flex flex-col items-center justify-between">
              <input
                type="range"
                min="-60"
                max="10"
                step="0.5"
                value={ch2.faderDb}
                onChange={(e) => updateCh(2, 'faderDb', parseFloat(e.target.value))}
                className="h-full -rotate-90 w-28 my-auto appearance-none bg-zinc-800 rounded accent-emerald-500 cursor-pointer"
              />
              <span className="text-[11px] font-mono font-bold text-zinc-300">
                {ch2.faderDb <= -60
                  ? '-INF'
                  : `${ch2.faderDb > 0 ? '+' : ''}${ch2.faderDb.toFixed(1)} dB`}
              </span>
            </div>
          </div>

          {/* Mute & Solo Buttons */}
          <div className="grid grid-cols-2 gap-1 pt-1 border-t border-zinc-800/80">
            <button
              onClick={() => updateCh(2, 'muted', !ch2.muted)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch2.muted
                  ? 'bg-red-600 text-white shadow-sm shadow-red-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              MUTE
            </button>
            <button
              onClick={() => updateCh(2, 'solo', !ch2.solo)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch2.solo
                  ? 'bg-amber-500 text-black shadow-sm shadow-amber-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              SOLO
            </button>
          </div>
        </div>

        {/* CH3/4 MEDIA (Stereo) */}
        <div className="bg-zinc-950/70 border border-zinc-800/80 rounded-md p-3 flex flex-col justify-between">
          <div className="text-center border-b border-zinc-800 pb-2">
            <div className="text-xs font-bold text-violet-300">CH3/4 MEDIA</div>
            <div className="text-[10px] text-zinc-500 font-mono">In 1-2 (Stereo)</div>
          </div>

          {/* Gain Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">GAIN</label>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value={ch34.gainDb}
              onChange={(e) => updateCh(3, 'gainDb', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-emerald-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {ch34.gainDb > 0 ? `+${ch34.gainDb.toFixed(1)}` : ch34.gainDb.toFixed(1)} dB
            </span>
          </div>

          {/* Balance Knob */}
          <div className="flex flex-col items-center py-1">
            <label className="text-[10px] font-bold text-zinc-400">BAL</label>
            <input
              type="range"
              min="-1"
              max="1"
              step="0.05"
              value={ch34.panOrBalance}
              onChange={(e) => updateCh(3, 'panOrBalance', parseFloat(e.target.value))}
              className="w-20 h-1.5 bg-zinc-800 rounded-lg appearance-none cursor-pointer accent-violet-500"
            />
            <span className="text-[10px] text-zinc-400 font-mono">
              {Math.abs(ch34.panOrBalance) < 0.05
                ? 'BAL C'
                : ch34.panOrBalance < 0
                ? `L${Math.round(Math.abs(ch34.panOrBalance) * 100)}`
                : `R${Math.round(ch34.panOrBalance * 100)}`}
            </span>
          </div>

          {/* Meter & Fader Section */}
          <div className="flex-1 flex space-x-2 my-2 min-h-[140px]">
            <div className="w-8 h-full">
              {renderMeter(ch34.peakL, ch34.peakR, true, ch34.clipped)}
            </div>
            <div className="flex-1 flex flex-col items-center justify-between">
              <input
                type="range"
                min="-60"
                max="10"
                step="0.5"
                value={ch34.faderDb}
                onChange={(e) => updateCh(3, 'faderDb', parseFloat(e.target.value))}
                className="h-full -rotate-90 w-28 my-auto appearance-none bg-zinc-800 rounded accent-violet-500 cursor-pointer"
              />
              <span className="text-[11px] font-mono font-bold text-zinc-300">
                {ch34.faderDb <= -60
                  ? '-INF'
                  : `${ch34.faderDb > 0 ? '+' : ''}${ch34.faderDb.toFixed(1)} dB`}
              </span>
            </div>
          </div>

          {/* Mute & Solo Buttons */}
          <div className="grid grid-cols-2 gap-1 pt-1 border-t border-zinc-800/80">
            <button
              onClick={() => updateCh(3, 'muted', !ch34.muted)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch34.muted
                  ? 'bg-red-600 text-white shadow-sm shadow-red-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              MUTE
            </button>
            <button
              onClick={() => updateCh(3, 'solo', !ch34.solo)}
              className={`py-1 text-[10px] font-bold rounded transition-colors ${
                ch34.solo
                  ? 'bg-amber-500 text-black shadow-sm shadow-amber-500/50'
                  : 'bg-zinc-800 text-zinc-400 hover:bg-zinc-700'
              }`}
            >
              SOLO
            </button>
          </div>
        </div>

        {/* MASTER STRIP */}
        <div className="bg-stone-950 border border-stone-800 rounded-md p-3 flex flex-col justify-between">
          <div className="text-center border-b border-stone-800 pb-2">
            <div className="text-xs font-bold text-rose-400">MASTER</div>
            <div className="text-[10px] text-zinc-500 font-mono">Out 1/2 (Main)</div>
          </div>

          {/* Dual Stereo Master Meters & Master Fader */}
          <div className="flex-1 flex space-x-3 my-4 min-h-[220px]">
            <div className="w-10 h-full">
              {renderMeter(master.peakL, master.peakR, true, master.clipped)}
            </div>
            <div className="flex-1 flex flex-col items-center justify-between">
              <input
                type="range"
                min="-60"
                max="10"
                step="0.5"
                value={master.faderDb}
                onChange={(e) => updateMaster('faderDb', parseFloat(e.target.value))}
                className="h-full -rotate-90 w-36 my-auto appearance-none bg-stone-800 rounded accent-rose-500 cursor-pointer"
              />
              <span className="text-xs font-mono font-bold text-rose-300">
                {master.faderDb <= -60
                  ? '-INF'
                  : `${master.faderDb > 0 ? '+' : ''}${master.faderDb.toFixed(1)} dB`}
              </span>
            </div>
          </div>

          {/* Master Mute */}
          <div className="pt-1 border-t border-stone-800">
            <button
              onClick={() => updateMaster('muted', !master.muted)}
              className={`w-full py-1.5 text-[11px] font-bold rounded transition-colors ${
                master.muted
                  ? 'bg-rose-600 text-white shadow-sm shadow-rose-500/50'
                  : 'bg-stone-800 text-stone-300 hover:bg-stone-700'
              }`}
            >
              MASTER MUTE
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
