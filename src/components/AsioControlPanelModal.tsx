import React, { useState } from 'react';
import { Sliders, CheckCircle2, X } from 'lucide-react';

interface AsioControlPanelModalProps {
  isOpen: boolean;
  onClose: () => void;
  deviceName: string;
  bufferSize: number;
  sampleRate: number;
  onBufferSizeChange: (newSize: number) => void;
}

export const AsioControlPanelModal: React.FC<AsioControlPanelModalProps> = ({
  isOpen,
  onClose,
  deviceName,
  bufferSize,
  sampleRate,
  onBufferSizeChange,
}) => {
  const [clockSource, setClockSource] = useState('Internal');
  const [safeMode, setSafeMode] = useState(true);

  if (!isOpen) return null;

  const bufferOptions = [32, 64, 128, 256, 512, 1024];

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/70 backdrop-blur-xs p-4">
      <div className="w-full max-w-md bg-zinc-900 border border-zinc-700 rounded-lg shadow-2xl overflow-hidden font-sans text-zinc-100 animate-in fade-in zoom-in-95 duration-150">
        {/* Windows title bar */}
        <div className="flex items-center justify-between px-3 py-2 bg-zinc-800 border-b border-zinc-700 select-none">
          <div className="flex items-center space-x-2 text-xs font-semibold text-zinc-200">
            <Sliders className="w-4 h-4 text-emerald-400" />
            <span>{deviceName} - ASIO Control Panel</span>
          </div>
          <button
            onClick={onClose}
            className="p-1 hover:bg-zinc-700 rounded text-zinc-400 hover:text-white transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Panel content */}
        <div className="p-5 space-y-5 text-sm">
          <div>
            <label className="block text-xs font-medium text-zinc-400 uppercase tracking-wider mb-2">
              ASIO Buffer Size (Samples)
            </label>
            <div className="grid grid-cols-6 gap-1.5 bg-zinc-950 p-1.5 rounded border border-zinc-800">
              {bufferOptions.map((size) => (
                <button
                  key={size}
                  onClick={() => onBufferSizeChange(size)}
                  className={`py-1.5 text-xs font-mono font-medium rounded transition-all ${
                    bufferSize === size
                      ? 'bg-emerald-600 text-white shadow-xs font-bold'
                      : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800'
                  }`}
                >
                  {size}
                </button>
              ))}
            </div>
            <p className="mt-1 text-xs text-zinc-500 font-mono">
              Calculated Buffer Latency: {((bufferSize / sampleRate) * 1000).toFixed(2)} ms @ {sampleRate} Hz
            </p>
          </div>

          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="block text-xs font-medium text-zinc-400 uppercase tracking-wider mb-1.5">
                Clock Source
              </label>
              <select
                value={clockSource}
                onChange={(e) => setClockSource(e.target.value)}
                className="w-full bg-zinc-950 border border-zinc-700 text-zinc-200 rounded px-2.5 py-1.5 text-xs focus:border-emerald-500 focus:outline-hidden"
              >
                <option value="Internal">Internal (Locked)</option>
                <option value="S/PDIF">S/PDIF Coaxial</option>
                <option value="ADAT">ADAT Optical</option>
                <option value="WordClock">Word Clock BNC</option>
              </select>
            </div>

            <div>
              <label className="block text-xs font-medium text-zinc-400 uppercase tracking-wider mb-1.5">
                Driver Bit Depth
              </label>
              <div className="bg-zinc-950 border border-zinc-800 text-zinc-300 rounded px-2.5 py-1.5 text-xs font-mono">
                32-bit Float / 24-bit PCM
              </div>
            </div>
          </div>

          <div className="pt-2 border-t border-zinc-800">
            <label className="flex items-center space-x-2.5 cursor-pointer select-none">
              <input
                type="checkbox"
                checked={safeMode}
                onChange={(e) => setSafeMode(e.target.checked)}
                className="rounded bg-zinc-950 border-zinc-700 text-emerald-600 focus:ring-0"
              />
              <span className="text-xs text-zinc-300">
                Safe Mode (Double buffer guard against driver dropouts)
              </span>
            </label>
          </div>

          <div className="flex items-center justify-between pt-3 border-t border-zinc-800">
            <div className="flex items-center space-x-1 text-xs text-emerald-400">
              <CheckCircle2 className="w-3.5 h-3.5" />
              <span>Steinberg ASIO v2.3 Compliant</span>
            </div>
            <button
              onClick={onClose}
              className="px-4 py-1.5 bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-medium rounded transition-colors"
            >
              Apply & Close
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
