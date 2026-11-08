/** Native pixels are useful when the view magnifies beyond the live tier.
 * Exports always request their own full render, independent of this policy.
 */
export function needsNativeDetail(nativeEdge: number, previewEdge: number, zoom: number, policy: 'zoom' | 'idle'): boolean {
  return nativeEdge > previewEdge && (policy === 'idle' || zoom > previewEdge / nativeEdge * 1.05);
}
