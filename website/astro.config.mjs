import { defineConfig } from 'astro/config';
export default defineConfig({ output:'static', trailingSlash:'always', site:process.env.SITE_ORIGIN || 'https://floatlet.vercel.app', vite:{build:{assetsInlineLimit:0}}, server:{port:4321}, devToolbar:{enabled:false} });
