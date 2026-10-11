import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Compañero · Panel del ESP32",
  description: "Gestioná tu dispositivo Compañero, sus tareas y preferencias.",
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="es-AR"><body>{children}</body></html>;
}
