import { HMI_PANEL_H, HMI_PANEL_W } from './panel';

const SVG_NS = 'http://www.w3.org/2000/svg';

const SVG_STYLE = `
.hmi-plate{fill:#fff;font-size:7px;font-weight:700;font-family:'Segoe UI',Arial,sans-serif}
.hmi-val{fill:#0f172a;font-size:11px;font-weight:700;font-family:'Segoe UI',Arial,sans-serif}
.hmi-chip-text{fill:#0f766e;font-size:10px;font-weight:700;font-family:'Segoe UI',Arial,sans-serif}
.hmi-name{fill:#334155;font-size:11px;font-weight:600;font-family:'Segoe UI',Arial,sans-serif}
`;

function waitFrame(): Promise<void> {
  return new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(() => r())));
}

function snapshotSvg(svg: SVGSVGElement, w: number, h: number): Promise<string> {
  const clone = svg.cloneNode(true) as SVGSVGElement;
  clone.setAttribute('width', String(w));
  clone.setAttribute('height', String(h));
  clone.setAttribute('xmlns', SVG_NS);
  const style = document.createElementNS(SVG_NS, 'style');
  style.textContent = SVG_STYLE;
  clone.insertBefore(style, clone.firstChild);
  const xml = new XMLSerializer().serializeToString(clone);
  const url = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(xml)}`;
  return new Promise((resolve, reject) => {
    const img = new Image();
    const timer = window.setTimeout(() => {
      img.src = '';
      reject(new Error('snapshot_failed'));
    }, 4000);
    img.onload = () => {
      window.clearTimeout(timer);
      const canvas = document.createElement('canvas');
      canvas.width = w;
      canvas.height = h;
      const ctx = canvas.getContext('2d');
      if (!ctx) {
        reject(new Error('snapshot_failed'));
        return;
      }
      ctx.fillStyle = '#d5dbe3';
      ctx.fillRect(0, 0, w, h);
      ctx.drawImage(img, 0, 0, w, h);
      resolve(canvas.toDataURL('image/png'));
    };
    img.onerror = () => {
      window.clearTimeout(timer);
      reject(new Error('snapshot_failed'));
    };
    img.src = url;
  });
}

function snapshotHtml(el: HTMLElement, w: number, h: number): string {
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const ctx = canvas.getContext('2d');
  if (!ctx) throw new Error('snapshot_failed');
  ctx.fillStyle = '#d5dbe3';
  ctx.fillRect(0, 0, w, h);
  const origin = el.getBoundingClientRect();
  const tiles = [...el.querySelectorAll('.hmi-tile')] as HTMLElement[];
  const cards = tiles.length ? tiles : [el];
  for (const t of cards) {
    const r = t.getBoundingClientRect();
    const x = r.left - origin.left;
    const y = r.top - origin.top;
    ctx.fillStyle = '#fff';
    ctx.strokeStyle = '#cbd5e1';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(x, y, r.width, r.height, 8);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = '#0f172a';
    ctx.font = '600 14px Segoe UI, Arial, sans-serif';
    const lines = (t.innerText || '').split('\n').map((s) => s.trim()).filter(Boolean);
    lines.forEach((line, i) => ctx.fillText(line, x + 12, y + 24 + i * 20, Math.max(40, r.width - 24)));
  }
  return canvas.toDataURL('image/png');
}

/** Rasterize a 10" panel frame (SVG unit or HTML rooms) to a PNG. */
export async function snapshotPanel(el: HTMLElement, w = HMI_PANEL_W, h = HMI_PANEL_H): Promise<string> {
  await waitFrame();
  const svg = el.querySelector('svg.hmi-svg, svg.hmi-svg--fit');
  if (svg instanceof SVGSVGElement) return snapshotSvg(svg, w, h);
  return snapshotHtml(el, w, h);
}
