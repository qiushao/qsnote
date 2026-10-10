// Use the site's installed Hexo and plugins for both preview and export.
const Hexo = require('hexo');
const hexo = new Hexo(process.cwd(), { silent: false });
// Hexo logs some plugin/template processing failures without rejecting generate().
let failed = false;
const logError = hexo.log.error.bind(hexo.log);
hexo.log.error = (...args) => {
  failed = true;
  logError(...args);
};
(async () => {
  await hexo.init();
  await hexo.call('clean');
  await hexo.call('generate', { bail: true });
  if (failed) throw new Error('Hexo reported errors; the site was not generated successfully.');
  await hexo.exit();
})().catch(async error => {
  console.error(error);
  await hexo.exit(error);
  process.exitCode = 1;
});
