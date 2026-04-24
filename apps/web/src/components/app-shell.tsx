import { PropsWithChildren } from "react";

import { SideNav } from "@/components/side-nav";
import { TopBar } from "@/components/top-bar";

type Props = PropsWithChildren<{
  title: string;
  subtitle: string;
}>;

export function AppShell({ title, subtitle, children }: Props) {
  return (
    <div className="flex min-h-screen bg-transparent">
      <SideNav />
      <div className="flex min-h-screen flex-1 flex-col">
        <TopBar title={title} subtitle={subtitle} />
        <main className="flex-1 px-4 py-6 lg:px-6">{children}</main>
      </div>
    </div>
  );
}
