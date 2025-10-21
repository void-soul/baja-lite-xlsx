/**
 * 验证预编译包 URL 是否正确
 */

const packageJson = require('./package.json');

const name = packageJson.name;
const version = packageJson.version;
const binary = packageJson.binary;

// 模拟 prebuild-install 的变量替换
function buildUrl(runtime, abi, platform, arch) {
  let url = binary.host;
  url += binary.remote_path
    .replace('{version}', 'v' + version);
  url += '/' + binary.package_name
    .replace('{name}', name)
    .replace('{version}', version)
    .replace('{runtime}', runtime)
    .replace('{abi}', abi)
    .replace('{platform}', platform)
    .replace('{arch}', arch);
  
  return url;
}

console.log('📦 URL 验证\n');
console.log('配置信息:');
console.log(`  name: ${name}`);
console.log(`  version: ${version}`);
console.log(`  package_name: ${binary.package_name}\n`);

console.log('生成的 URL:\n');

const testCases = [
  { runtime: 'napi', abi: '8', platform: 'win32', arch: 'x64' },
  { runtime: 'electron', abi: '34.0', platform: 'win32', arch: 'x64' }
];

testCases.forEach(config => {
  const url = buildUrl(config.runtime, config.abi, config.platform, config.arch);
  console.log(`${config.runtime.toUpperCase()}:`);
  console.log(`  ${url}\n`);
});

// 对比实际存在的 URL
console.log('GitHub Release 实际文件（v1.0.9）:');
console.log('  https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.9/baja-lite-xlsx-v1.0.9-napi-v8-win32-x64.tar.gz\n');

// 当前版本应该生成的 URL
const expectedUrl = buildUrl('napi', '8', 'win32', 'x64');
const actualUrl = 'https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.9/baja-lite-xlsx-v1.0.9-napi-v8-win32-x64.tar.gz';

console.log('格式匹配检查:');
if (version === '1.0.9') {
  if (expectedUrl === actualUrl) {
    console.log('  ✅ 格式完全匹配！');
  } else {
    console.log('  ❌ 格式不匹配！');
    console.log(`  预期: ${expectedUrl}`);
    console.log(`  实际: ${actualUrl}`);
  }
} else {
  console.log(`  ⚠️  当前版本是 ${version}，测试的是 1.0.9`);
  console.log(`  新版本将生成: ${expectedUrl}`);
}

