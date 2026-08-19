import { useCallback, useRef, useState } from "react";
import { UploadCloud } from "lucide-react";

interface DropzoneProps {
  onFileSelected: (file: File) => void;
  disabled?: boolean;
}

export function Dropzone({ onFileSelected, disabled }: DropzoneProps) {
  const [isDragActive, setIsDragActive] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  const handleFiles = useCallback(
    (files: FileList | null) => {
      if (!files || files.length === 0 || disabled) return;
      onFileSelected(files[0]);
    },
    [onFileSelected, disabled],
  );

  return (
    <div
      role="button"
      tabIndex={0}
      aria-disabled={disabled}
      onClick={() => !disabled && inputRef.current?.click()}
      onKeyDown={(e) => {
        if (!disabled && (e.key === "Enter" || e.key === " ")) inputRef.current?.click();
      }}
      onDragOver={(e) => {
        e.preventDefault();
        if (!disabled) setIsDragActive(true);
      }}
      onDragLeave={() => setIsDragActive(false)}
      onDrop={(e) => {
        e.preventDefault();
        setIsDragActive(false);
        handleFiles(e.dataTransfer.files);
      }}
      className="flex flex-col items-center justify-center gap-4 rounded-2xl px-8 py-16 text-center transition-colors"
      style={{
        border: `2px dashed ${isDragActive ? "var(--series-blue)" : "var(--border-hairline)"}`,
        background: isDragActive ? "var(--surface-2)" : "var(--surface-1)",
        cursor: disabled ? "not-allowed" : "pointer",
        opacity: disabled ? 0.6 : 1,
      }}
    >
      <input
        ref={inputRef}
        type="file"
        className="hidden"
        accept="image/*,application/pdf,text/plain,.txt,audio/wav,.wav"
        onChange={(e) => handleFiles(e.target.files)}
        disabled={disabled}
      />
      <UploadCloud size={40} style={{ color: "var(--series-blue)" }} strokeWidth={1.5} aria-hidden="true" />
      <div>
        <p className="font-medium" style={{ color: "var(--text-primary)" }}>
          Drop a file here, or click to browse
        </p>
        <p className="mt-1 text-sm" style={{ color: "var(--text-muted)" }}>
          Supports images (JPEG, PNG, WebP, BMP), PDFs, plain text, and WAV audio. Video is on the roadmap.
        </p>
      </div>
    </div>
  );
}
