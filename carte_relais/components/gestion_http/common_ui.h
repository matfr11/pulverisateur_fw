/**
 * @file common_ui.h
 * @brief Styles CSS communs pour l'interface web
 */

#ifndef COMMON_UI_H
#define COMMON_UI_H

const char COMMON_STYLE[] = R"=====(
<style>
  /* Base & Reset */
  body { margin: 0; padding: 0; background: #121212; color: white; font-family: sans-serif; height: 100vh; width: 100vw; overflow: hidden; display: flex; flex-direction: column; }
  
  /* BANDEAU SUPÉRIEUR */
  .top-bar { 
    display: flex; 
    align-items: center; 
    justify-content: space-between; 
    background: #1a1a1a; 
    padding: 0 12px; 
    border-bottom: 1px solid #333; 
    height: 38px;
  }
  #status-tag { font-size: 0.65rem; font-weight: bold; padding: 3px 8px; border-radius: 4px; }
  .ver-info { font-size: 0.6rem; color: #444; }
  .settings-icon { color: #666; text-decoration: none; font-size: 1.1rem; }
  .online { background: #1a592e; color: #2ecc71; }
  .offline { background: #591a1a; color: #e74c3c; }

  /* STRUCTURE DASHBOARD */
  .dashboard { display: flex; flex: 1; width: 100%; overflow-x: auto; scroll-snap-type: x mandatory; scroll-behavior: smooth; }
  .slide { flex: 0 0 100vw; height: 100%; scroll-snap-align: start; box-sizing: border-box; padding: 8px; background: #1a1a1a; overflow-y: auto; }

  /* TITRES DE SLIDES */
  h2 { 
    color: #ffffff; 
    text-align: center; 
    font-size: 1rem; 
    margin: 4px 0 10px 0;
    text-transform: uppercase;
    letter-spacing: 1.5px;
    border-bottom: 2px solid #3399ff;
    width: 100%;
    padding-bottom: 3px;
  }

  /* COMPOSANTS CARTES ET GAUGES */
  .control-card { 
    background: #262626; 
    border-radius: 10px; 
    padding: 6px 8px;
    margin-bottom: 6px;
    border: 1px solid #383838; 
  }
  .card-title { font-size: 0.65rem; color: #777; margin-bottom: 3px; text-transform: uppercase; text-align: center; }
  
  .h-gauge-container { width: 100%; height: 20px; background: #000; border-radius: 5px; border: 1px solid #444; position: relative; overflow: hidden; margin-bottom: 4px; }
  .h-gauge-fill { height: 100%; width: 0%; transition: width 0.5s ease; position: absolute; }
  .h-gauge-text { position: absolute; width: 100%; text-align: center; line-height: 20px; font-weight: bold; z-index: 2; color: white; text-shadow: 1px 1px 2px black; font-size: 0.75rem; }

  /* BOUTONS */
  .btn-full { 
    width: 100%; 
    padding: 14px;
    border-radius: 8px; 
    border: none; 
    background: #333; 
    color: #eee; 
    font-weight: bold; 
    cursor: pointer; 
    margin-bottom: 4px; 
    font-size: 0.8rem; 
  }
  .btn-group { display: flex; gap: 4px; background: #121212; padding: 3px; border-radius: 6px; }
  .btn-v { flex: 1; padding: 12px 2px; border: none; border-radius: 4px; background: #333; color: #666; font-weight: bold; cursor: pointer; font-size: 0.75rem; }

  /* COULEURS ET ÉTATS */
  .active-green { background: #2ecc71 !important; color: white !important; }
  .active-red { background: #e74c3c !important; color: white !important; }
  .active-blue { background: #3399ff !important; color: white !important; }
  .active-yellow { background: #f1c40f !important; color: black !important; }
  .active-off { background: #444 !important; color: white !important; }    

  /* ALERTE BANDEAU */
  .alert-mini {
      background: #e74c3c;
      color: white;
      padding: 1px 6px;
      border-radius: 4px;
      font-size: 0.65em;
      font-weight: bold;
      animation: blink 1s infinite;
      margin-left: auto;
      margin-right: 5px;
  }
  @keyframes blink { 50% { opacity: 0.3; } }
  .blink-border { border: 2px solid #e74c3c; animation: blink 1s infinite; }

  /* MODE PAYSAGE */
  @media (orientation: landscape) {
      .dashboard { 
          overflow-x: hidden !important; 
          scroll-snap-type: none !important; 
          display: flex !important;
      }
      
      .slide { 
          height: 100% !important;
          padding: 5px !important;
          border-right: 1px solid #333;
      }

      .slide:nth-child(1), .slide:nth-child(3) { 
          flex: 2 1 40%; 
      }

      .slide:nth-child(2) { 
          flex: 1 1 20%; 
          min-width: 180px;
          background: #151515 !important;
      }

      .slide:nth-child(2) .btn-full {
          font-size: 0.7rem;
          padding: 10px 5px;
      }
      
      h2 { font-size: 0.85rem; margin-bottom: 5px; }
  }
</style>
)=====";

#endif // COMMON_UI_H
