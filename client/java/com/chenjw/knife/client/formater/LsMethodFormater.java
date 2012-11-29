package com.chenjw.knife.client.formater;

import com.chenjw.knife.core.Printer;
import com.chenjw.knife.core.Printer.Level;
import com.chenjw.knife.core.model.ClassMethodInfo;
import com.chenjw.knife.core.model.MethodInfo;
import com.chenjw.knife.utils.StringHelper;

public class LsMethodFormater extends GrepPrintFormater {

	private ClassMethodInfo classMethodInfo;

	public LsMethodFormater(Printer printer, String grep) {
		super(printer, grep);
	}

	@Override
	public void print(Level level) {
		PreparedTableFormater table = new PreparedTableFormater(printer, grep);
		table.setTitle("idx", "type", "method");
		MethodInfo[] methodInfos = classMethodInfo.getMethods();
		if (methodInfos != null) {
			int i = 0;
			for (MethodInfo method : methodInfos) {
				table.addLine(
						String.valueOf(i),
						method.isStatic() ? "[static-method]" : "[method]",
						method.getName()
								+ "("
								+ StringHelper.join(
										method.getParamClassNames(), ",") + ")");
			}
			i++;
		}
		table.print(level);
		this.print(level, "finished!");
	}

}