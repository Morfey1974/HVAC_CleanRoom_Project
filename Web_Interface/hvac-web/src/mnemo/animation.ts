import { useEffect, useRef } from 'react';

type Tick = (dt: number) => void;
const subscribers = new Set<Tick>();
let last = 0;
let running = false;

function loop(now: number) {
  const dt = last ? Math.min((now - last) / 1000, 0.1) : 0;
  last = now;
  subscribers.forEach((s) => s(dt));
  if (subscribers.size) requestAnimationFrame(loop);
  else {
    running = false;
    last = 0;
  }
}

/** One shared animation frame loop for all elements; callbacks write to the DOM directly (no re-render). */
function useTick(cb: Tick) {
  const ref = useRef(cb);
  ref.current = cb;
  useEffect(() => {
    const s: Tick = (dt) => ref.current(dt);
    subscribers.add(s);
    if (!running) {
      running = true;
      requestAnimationFrame(loop);
    }
    return () => {
      subscribers.delete(s);
    };
  }, []);
}

/** Continuous rotation of a <g> around (cx, cy) at degPerSec. */
export function useSpin<T extends SVGGraphicsElement>(degPerSec: number, cx = 0, cy = 0) {
  const ref = useRef<T>(null);
  const angle = useRef(0);
  useTick((dt) => {
    if (!degPerSec || !ref.current) return;
    angle.current = (angle.current + degPerSec * dt) % 360;
    ref.current.setAttribute('transform', `rotate(${angle.current} ${cx} ${cy})`);
  });
  return ref;
}

/** Moving dashes along a path at pxPerSec (air/water flow). */
export function useDash<T extends SVGGeometryElement>(pxPerSec: number) {
  const ref = useRef<T>(null);
  const offset = useRef(0);
  useTick((dt) => {
    if (!pxPerSec || !ref.current) return;
    offset.current -= pxPerSec * dt;
    ref.current.style.strokeDashoffset = String(offset.current);
  });
  return ref;
}
