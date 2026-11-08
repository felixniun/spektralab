import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.goto('/?open=');
  await expect(page.locator('.thumb')).toHaveCount(8);
  await expect(page.getByTestId('tier-badge')).toHaveText('full', { timeout: 20000 });
});

test('panel grips resize, clamp, persist and reset the Mac layout', async ({ page }) => {
  const grip = page.getByRole('separator', { name: 'Resize film panel' });
  const bounds = (await grip.boundingBox())!;
  await page.mouse.move(bounds.x + 3, bounds.y + 100);
  await page.mouse.down();
  await page.mouse.move(bounds.x + 75, bounds.y + 100, { steps: 8 });
  await page.mouse.up();
  await expect(grip).toHaveAttribute('aria-valuenow', '326');
  await page.reload();
  await expect(grip).toHaveAttribute('aria-valuenow', '326');
  await grip.focus();
  await page.keyboard.press('End');
  await expect(grip).toHaveAttribute('aria-valuenow', '380');
  await grip.dblclick();
  await expect(grip).toHaveAttribute('aria-valuenow', '254');
  await page.getByRole('button', { name: 'Hide filmstrip', exact: true }).click();
  await expect(page.getByTestId('filmstrip')).toHaveCount(0);
  await page.getByRole('button', { name: 'Show filmstrip', exact: true }).click();
  await expect(page.getByTestId('filmstrip')).toBeVisible();
});

test('internal thumbnail drag preserves the library and open image, then survives reopening', async ({ page }) => {
  const cells = page.locator('.thumb');
  const original = await cells.evaluateAll((els) => els.map((e) => e.getAttribute('data-path')));
  const a = (await cells.nth(0).boundingBox())!;
  const c = (await cells.nth(2).boundingBox())!;
  await page.mouse.move(a.x + a.width / 2, a.y + a.height / 2);
  await page.mouse.down();
  await page.mouse.move(c.x + c.width / 2, c.y + c.height / 2, { steps: 12 });
  await page.mouse.up();
  await expect(cells).toHaveCount(8);
  await expect(cells.nth(2)).toHaveAttribute('data-path', original[0]!);
  await expect(page.locator('.thumb.open')).toHaveAttribute('data-path', original[0]!);
  await expect.poll(() => page.evaluate(() => Object.keys(localStorage).some((key) => key.startsWith('store:Order/')))).toBe(true);
  await page.reload();
  await expect(cells.nth(2)).toHaveAttribute('data-path', original[0]!);
  await cells.nth(2).focus();
  await page.keyboard.press('Alt+ArrowLeft');
  await expect(cells.nth(1)).toHaveAttribute('data-path', original[0]!);
});

test('fold controls move between rail headers and toolbar', async ({ page }, testInfo) => {
  const left = page.getByTestId('left-rail');
  await left.getByRole('button', { name: 'Hide the darkroom rail' }).click();
  await expect(left).toHaveCount(0);
  await page.getByTestId('topbar').getByRole('button', { name: 'Show the darkroom rail' }).click();
  await expect(left).toBeVisible();
  await page.screenshot({ path: testInfo.outputPath('mac-parity-editor.png') });
});

test('export grid keeps the worklist and remembers its view and selected recipe', async ({ page }, testInfo) => {
  await page.keyboard.press('Control+a');
  await page.keyboard.press('Control+e');
  const exportPage = page.getByTestId('export-page');
  await expect(exportPage).toBeVisible();
  await exportPage.getByRole('button', { name: 'Grid', exact: true }).click();
  const grid = page.getByTestId('export-grid');
  await expect(grid.locator('.export-cell')).toHaveCount(8);
  await grid.locator('.export-cell').nth(3).click();
  await expect(grid.locator('.export-cell.open')).toContainText('DSC_0004.ARW');
  await exportPage.getByRole('button', { name: 'Add a recipe', exact: true }).click();
  const name = exportPage.getByRole('textbox', { name: 'Recipe name' });
  await name.fill('My print');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('store:Export/recipes.json'))).toContain('My print');
  await page.screenshot({ path: testInfo.outputPath('export-grid.png') });
  await page.reload();
  await expect(page.locator('.thumb')).toHaveCount(8);
  await expect(page.getByTestId('tier-badge')).toHaveText('full', { timeout: 20000 });
  await page.keyboard.press('Control+a');
  await page.keyboard.press('Control+e');
  await expect(grid).toBeVisible();
  await expect(name).toHaveValue('My print');
  await expect(grid.locator('.export-cell')).toHaveCount(8);
  await exportPage.getByRole('button', { name: 'Viewer', exact: true }).click();
  await expect(page.getByTestId('export-proof')).toBeVisible();
  await expect(exportPage).toContainText('Canvas preview · sRGB');
});

test('stopping a mock batch reports the completed count rather than the whole batch', async ({ page }) => {
  await page.goto('/?open=&renderDelay=150');
  await expect(page.getByTestId('tier-badge')).toHaveText('full', { timeout: 20000 });
  await page.keyboard.press('Control+a');
  await page.keyboard.press('Control+e');
  await page.getByTestId('export-run').click();
  await page.getByRole('button', { name: 'Stop', exact: true }).click();
  await expect(page.getByTestId('export-run')).toBeVisible({ timeout: 20000 });
  const progress = page.getByRole('progressbar', { name: 'Export progress' });
  await expect(progress).toHaveAttribute('max', '8');
  expect(Number(await progress.getAttribute('value'))).toBeLessThan(8);
  await expect(page.getByTestId('export-page')).toContainText('Stopped');
});

test('an unavailable engine fails the batch promptly without waiting ten minutes per photo', async ({ page }) => {
  await page.keyboard.press('Control+a');
  await page.keyboard.press('Control+e');
  await page.evaluate(() => {
    const store = (window as unknown as { __spk: { sessionStore: { setState(value: unknown): void } } }).__spk.sessionStore;
    store.setState({ engineSession: null, hostState: { phase: 'starting' } });
  });
  await page.getByTestId('export-run').click();
  await expect(page.getByTestId('export-page').getByRole('status')).toHaveText('Written 0 · Skipped 0 · Failed 8', { timeout: 10000 });
  await expect(page.getByRole('progressbar', { name: 'Export progress' })).toHaveAttribute('value', '8');
});

test('slide selection and reopening an old sidecar always enable direct scan', async ({ page }) => {
  // Seed after the old document's beforeunload save, rather than racing it.
  await page.addInitScript(() => localStorage.setItem('sidecar:/mock/Roll 01/DSC_0001.NEF', JSON.stringify({ params: { filmStock: 'kodak_ektachrome_100', printStock: 'kodak_supra_endura', scanFilm: false } })));
  await page.reload();
  await expect(page.getByTestId('tier-badge')).toHaveText('full', { timeout: 20000 });
  const params = () => page.evaluate(() => (window as unknown as { __spk: { sessionStore: { getState(): { sidecar: { params: { scanFilm: boolean; printStock: string } } } } } }).__spk.sessionStore.getState().sidecar.params);
  expect(await params()).toMatchObject({ scanFilm: true, printStock: 'kodak_supra_endura' });
  for (const stock of ['fujifilm_provia_100f', 'fujifilm_velvia_100', 'kodak_ektachrome_100', 'kodak_kodachrome_64']) {
    await page.getByTestId('film-list').locator(`[data-stock="${stock}"]`).click();
    expect((await params()).scanFilm).toBe(true);
    await page.getByTestId('film-list').locator('[data-stock="kodak_portra_800"]').click();
    expect(await params()).toMatchObject({ scanFilm: false, printStock: 'kodak_portra_endura' });
  }
  await page.getByTestId('paper-list').locator('[data-stock="__positive"]').click();
  expect((await params()).scanFilm).toBe(true);
  await page.getByTestId('paper-list').locator('[data-stock="kodak_supra_endura"]').click();
  expect(await params()).toMatchObject({ scanFilm: false, printStock: 'kodak_supra_endura' });
});

test('a 500-photo roll only queues and paints thumbnails close to the visible strip', async ({ page }) => {
  await page.evaluate(() => {
    const store = (window as unknown as { __spk: { sessionStore: { setState(value: unknown): void } } }).__spk.sessionStore;
    store.setState({ frames: Array.from({ length: 500 }, (_, i) => ({ path: `/mock/Large/frame-${i}.NEF`, name: `frame-${i}.NEF`, size: 1, mtime_ms: 0 })) });
  });
  await expect(page.locator('.thumb')).toHaveCount(500);
  await expect.poll(() => page.locator('.thumb canvas').count()).toBeGreaterThan(0);
  expect(await page.locator('.thumb canvas').count()).toBeLessThan(30);
  const stats = await page.evaluate(() => (window as unknown as { __spk: { thumbs: { stats(): { queued: number } } } }).__spk.thumbs.stats());
  expect(stats.queued).toBeLessThan(30);
});
